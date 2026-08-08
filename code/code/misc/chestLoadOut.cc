//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD++ - All rights reserved, SneezyMUD Coding Team
//
//      "chestLoadOut.cc" - Random loot generation for zone containers
//
//////////////////////////////////////////////////////////////////////////

#include "chestLoadOut.h"

#include <algorithm>
#include <array>

#include "being.h"
#include "bulkLoadOut.h"
#include "db.h"
#include "disc_thief_looting.h"
#include "extern.h"
#include "handler.h"
#include "low.h"
#include "monster.h"
#include "obj_bag.h"
#include "obj_chest.h"
#include "obj_money.h"
#include "obj_open_container.h"
#include "room.h"
#include "shop.h"
#include "sys_loot.h"

namespace {

  // -----------------------------------------------------------------------
  // Fill chance, additive, applied to the containers canHoldLoot() accepts.
  //
  // Type alone is a poor signal: ITEM_CHEST is what builders reach for whenever
  // something has to hold things, so it also covers bookshelves, desks, coat
  // racks and a longberry bush. Price and max_exist are no better -- nearly
  // every container is price 1 and max_exist 9999, genuine chests included.
  //
  // A key is the one thing a builder sets deliberately, which makes it the best
  // available proxy for "this is meant to hold treasure". runResetCmdO already
  // keys on it when re-locking.
  // -----------------------------------------------------------------------
  inline constexpr int FILL_PCT_CONTAINER = 3;
  inline constexpr int FILL_PCT_CHEST = 5;
  inline constexpr int FILL_PCT_LOCKED = 5;
  inline constexpr int FILL_PCT_KEYED = 10;

  // A container on a mob starts where an unlocked chest does. Reaching it
  // means killing the mob first, which a chest on the floor never asks of you.
  inline constexpr int FILL_PCT_CARRIED = 5;

  // -----------------------------------------------------------------------
  // How many items a container settles at, by level.
  //
  // Room containers are never recreated -- runResetCmdO only runs the loader
  // at boot and later resets just re-lock -- so the same object survives for
  // the life of the process and would otherwise gain a fresh payload every
  // reset. Rather than refuse to touch a container that already holds
  // something, the odds taper toward zero as it approaches this ceiling, so it
  // drifts up to a level-appropriate size and settles there.
  //
  // Squared, so the curve turns on late: a low-level zone stays cheap to farm
  // while the top zones approach the ceiling. Zone levels run 2-78, median 23.
  //
  // The floor exists because builder-placed P contents count toward the total.
  // The median P load is 3 items and the mean is 6, so a bare curve would cap
  // a low-level zone below what the builder already put there and freeze those
  // containers out entirely -- which is the behaviour this replaces.
  // -----------------------------------------------------------------------
  inline constexpr int CAPACITY_FLOOR = 10;
  inline constexpr int CAPACITY_CEILING = 75;
  inline constexpr int CAPACITY_LEVEL_SCALE = 75;

  // Rolls scale with level: one per five levels, at least one.
  inline constexpr int LEVELS_PER_ROLL = 5;

  // One roll in ten comes from a level band above the zone's own.
  inline constexpr int OVERLEVEL_CHANCE_PCT = 10;
  inline constexpr int OVERLEVEL_MAX = 10;

  // Band the L loot table draws from, either side of the chest's level.
  inline constexpr int LOOT_LEVEL_SPREAD = 5;

  // The four rare potions. They are barred from the L table by name in
  // isLegalLoot(), so this is a deliberate second door rather than a
  // side effect of one -- removing them from that blocklist would let them
  // into every L load in the game, which is not what we want.
  //
  // TMonster::buffMobLoader is the only other source; it refuses below level
  // 40, so we do too. Without that floor a newbie-zone chest would hand out
  // stat potions at the same rate as a level 90 one.
  inline constexpr int RARE_POTION_ODDS = 100;
  inline constexpr int RARE_POTION_MIN_LEVEL = 40;

  // Mirrors TMonster::getLoadMoney's "typical mob has 1.5 * L^2 gold" curve.
  // A typical mob's moneyConst is 3; a chest is worth five of those.
  inline constexpr double CHEST_MONEY_CONST = 15.0;
  inline constexpr int CENTRAL_BANK = 123;

  // The six races spanning every size tier in raceSizeInfo(), so chest gear
  // isn't uniformly human-sized.
  inline constexpr std::array<race_t, 6> lootRaces = {RACE_HOBBIT, RACE_GNOME,
    RACE_DWARF, RACE_ELVEN, RACE_HUMAN, RACE_OGRE};

  // What a single roll produces. Weighted low: cash is the common case, gear
  // the prize.
  enum class LootKind {
    Cash,
    StealSet,
    LootTable,
    Gear
  };

  [[nodiscard]] LootKind rollLootKind() {
    switch (::number(1, 10)) {
      case 1:
      case 2:
      case 3:
      case 4:
        return LootKind::Cash;
      case 5:
      case 6:
      case 7:
        return LootKind::StealSet;
      case 8:
      case 9:
        return LootKind::LootTable;
      default:
        return LootKind::Gear;
    }
  }

  // Chests and plain bags only. Every other TOpenContainer subclass holds
  // something specific -- spellbags components, quivers arrows, keyrings keys,
  // moneypouches talens, card decks cards -- so general loot in one reads
  // wrong and risks confusing the object's own handling of its contents.
  // Wagons and cookware are likewise not treasure containers.
  [[nodiscard]] bool canHoldLoot(const TOpenContainer* cont) {
    return dynamic_cast<const TChest*>(cont) || dynamic_cast<const TBag*>(cont);
  }

  [[nodiscard]] int uniformRarePotion() {
    switch (::number(0, 3)) {
      case 0:
        return Obj::MYSTERY_POTION;
      case 1:
        return Obj::YOUTH_POTION;
      case 2:
        return Obj::STATS_POTION;
      default:
        return Obj::LEARNING_POTION;
    }
  }

  void addPotion(TOpenContainer* cont, int vnum) {
    if (TObj* pot = read_object(vnum, VIRTUAL))
      *cont += *pot;
  }

  // Deliberately not buffMobLoader's 5 : 1 : 1 : 12. That path already makes
  // characteristics the common drop, so repeating its bias here would just
  // duplicate it; chests lead with mystery and treat learning as the prize, and
  // the two sources complement each other instead.
  //
  // d20: 1-9 mystery, 10-14 youth, 15-17 characteristics, 18-19 learning,
  // 20 yields two potions, each drawn evenly from all four (so a 20 can pay
  // out the same potion twice).
  void maybeAddRarePotion(TOpenContainer* cont, int level) {
    if (level < RARE_POTION_MIN_LEVEL)
      return;
    if (::number(1, RARE_POTION_ODDS) != 1)
      return;

    const int roll = ::number(1, 20);
    if (roll <= 9)
      addPotion(cont, Obj::MYSTERY_POTION);
    else if (roll <= 14)
      addPotion(cont, Obj::YOUTH_POTION);
    else if (roll <= 17)
      addPotion(cont, Obj::STATS_POTION);
    else if (roll <= 19)
      addPotion(cont, Obj::LEARNING_POTION);
    else
      for (int i = 0; i < 2; ++i)
        addPotion(cont, uniformRarePotion());
  }

  [[nodiscard]] int containerCapacity(int level) {
    const double t = static_cast<double>(level) / CAPACITY_LEVEL_SCALE;
    return std::min(CAPACITY_CEILING,
      CAPACITY_FLOOR + static_cast<int>(CAPACITY_CEILING * t * t));
  }

  [[nodiscard]] int fillChance(const TOpenContainer* cont, bool carried) {
    int chance = FILL_PCT_CONTAINER;
    if (carried)
      chance += FILL_PCT_CARRIED;
    if (dynamic_cast<const TChest*>(cont))
      chance += FILL_PCT_CHEST;
    if (cont->isContainerFlag(CONT_LOCKED))
      chance += FILL_PCT_LOCKED;
    if (cont->getKeyNum() >= 0)
      chance += FILL_PCT_KEYED;
    return chance;
  }

  // Talens a chest of this level is worth, on the same curve and global
  // modifier the rest of the economy uses.
  [[nodiscard]] double chestMoney(int level) {
    double gold = level * std::max(20.0, static_cast<double>(level)) *
                  CHEST_MONEY_CONST * 7.5 / 10;
    return gold * shop_index[CENTRAL_BANK].getProfitSell(nullptr, nullptr);
  }

  // Hand the L loot table a synthetic reset command targeting this container.
  // Cheaper than widening sysLootLoad's signature, which the zone files depend
  // on at boot.
  void addLootTableItem(TOpenContainer* cont, int level) {
    resetCom rs;
    rs.command = 'L';
    rs.if_flag = 1;
    rs.arg1 = std::max(1, level - LOOT_LEVEL_SPREAD);
    rs.arg2 = level + LOOT_LEVEL_SPREAD;
    rs.arg3 = 0;
    rs.arg4 = 1;  // 1 = load onto the object we pass in

    sysLootLoad(rs, nullptr, cont, false);
  }

  void fillContainer(TOpenContainer* cont, int zoneLevel) {
    int level = zoneLevel;
    if (::number(1, 100) <= OVERLEVEL_CHANCE_PCT)
      level += ::number(1, OVERLEVEL_MAX);

    // One class per chest, so its contents read as a coherent stash rather than
    // one item per class. Ranger and commoner are dead classes.
    auto classInd =
      static_cast<classIndT>(::number(MAGE_LEVEL_IND, MONK_LEVEL_IND));
    unsigned short classBits = 1 << classInd;
    race_t race =
      lootRaces[::number(0, static_cast<int>(lootRaces.size()) - 1)];

    const double money = chestMoney(level);
    int rolls = ::number(1, std::max(1, level / LEVELS_PER_ROLL));

    // Cash accumulates and lands as a single pile. Separate piles in one
    // container is the problem runResetCmdP already guards against.
    double cash = 0.0;

    for (int i = 0; i < rolls; ++i) {
      switch (rollLootKind()) {
        case LootKind::Cash:
          cash += money;
          break;

        case LootKind::StealSet:
          if (TObj* obj = generateStealLoot(classBits, money)) {
            *cont += *obj;
            obj->onObjLoad();
          }
          break;

        case LootKind::LootTable:
          addLootTableItem(cont, level);
          break;

        case LootKind::Gear:
          if (TObj* obj = bulkLoadOutItem(classInd, level, race))
            *cont += *obj;
          break;
      }
    }

    if (cash >= 1.0) {
      if (TMoney* pile = create_money(static_cast<int>(cash)))
        *cont += *pile;
      else
        vlogf(LOG_BUG, "chestLoadOut: problem creating money");
    }

    // Independent of the roll table -- a bonus on top of whatever the chest
    // already produced, not a competing outcome that would crowd out the rest.
    maybeAddRarePotion(cont, level);
  }

  // Roll for one candidate container and fill it if the roll wins.
  void tryFillContainer(TThing* thing, int level, bool carried) {
    auto* cont = dynamic_cast<TOpenContainer*>(thing);
    if (!cont || !canHoldLoot(cont))
      return;

    // Scale the odds by how much room is left, so a container fills at full
    // rate when empty, slows as it approaches its ceiling, and stops there.
    // Emptying one restores its odds, which keeps a looted chest worth
    // revisiting without letting an untouched one grow without bound.
    const int cap = containerCapacity(level);
    const int room = cap - static_cast<int>(cont->stuff.size());
    if (room <= 0)
      return;

    if (::number(1, 100) > fillChance(cont, carried) * room / cap)
      return;

    fillContainer(cont, level);
  }

}  // anonymous namespace

void chestLoadOut(zoneData& zone) {
  // Same measure trapDoors() uses. Valid only once the reset loop has run, so
  // every mob in the zone has been counted.
  const int zoneLevel =
    zone.num_mobs ? static_cast<int>(zone.mob_levels / zone.num_mobs) : 0;
  if (zoneLevel <= 0)
    return;

  const int bottom = zone.zone_nr ? (zone_table[zone.zone_nr - 1].top + 1) : 0;

  for (int rnum = bottom; rnum <= zone.top; rnum++) {
    TRoom* rp = real_roomp(rnum);
    if (!rp)
      continue;

    for (TThing* thing : rp->stuff)
      tryFillContainer(thing, zoneLevel, false);
  }
}

void mobBagLoadOut(TMonster* mob) {
  if (!mob)
    return;

  const int level = mob->GetMaxLevel();
  if (level <= 0)
    return;

  for (TThing* thing : mob->stuff)
    tryFillContainer(thing, level, true);

  for (wearSlotT slot = MIN_WEAR; slot < MAX_WEAR; slot++)
    if (TThing* worn = mob->equipment[slot])
      tryFillContainer(worn, level, true);
}
