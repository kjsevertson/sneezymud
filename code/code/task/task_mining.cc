//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "task_mining.cc" - Cutting ore out of hills, mountains and caves
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "handler.h"
#include "extern.h"
#include "room.h"
#include "being.h"
#include "materials.h"
#include "obj_base_weapon.h"
#include "augment.h"
#include "bulkLoadOut.h"
#include "mining.h"

const int MINE_ORE_VNUM = 29545;

// A pick, held. There is no tool type for one, so the test is a weapon that
// answers to "pick" -- which covers the five in the world and the two a smith
// can forge, and excludes lockpicks, which are not weapons.
TBaseWeapon* getHeldPick(TBeing* ch) {
  for (int i = 0; i < MAX_WEAR; i++) {
    TBaseWeapon* weapon = dynamic_cast<TBaseWeapon*>(ch->equipment[i]);
    if (weapon && isname("pick", weapon->name))
      return weapon;
  }

  return nullptr;
}

// Hills, mountains and the three caves. Sewers run deeper than any of them and
// are deliberately not on the list: depth alone should not make a drain worth
// quarrying.
bool isMineableSector(const TRoom* rp) {
  if (!rp)
    return false;

  sectorTypeT sector = rp->getSectorType();

  return rp->isHillSector() || rp->isMountainSector() ||
         sector == SECT_ARCTIC_CAVE || sector == SECT_TEMPERATE_CAVE ||
         sector == SECT_TROPICAL_CAVE;
}

// How far below the surface, never negative.
int getMineDepth(const TRoom* rp) {
  return rp ? max(0, -rp->getZCoord()) : 0;
}

// The chance a room is worked out by an attempt: three fifths at the surface,
// ten points less for every three levels down, and never quite nothing -- a
// vein at the bottom of the world still gives out one time in a hundred.
int getMinedOutChance(const TRoom* rp) {
  return max(1, 61 - 10 * (getMineDepth(rp) / 3));
}

// What the rock holds. Nothing is impossible anywhere -- a hillside can give
// up a diamond -- but sector and depth move the odds a long way. Weights are
// picked from one table: what the hills, the mountains and the caves are each
// likely to be hiding, with a floor depth below which a thing does not appear
// at all.
//
// Alloys are absent on purpose: brass, bronze and steel are made, not dug.
// Starmetal is absent because it does not come out of the ground at all.
namespace {
  struct OreEntry {
      int material;
      int minDepth;  // nothing shallower than this ever holds it
      int hill;
      int mountain;
      int cave;
  };

  const OreEntry oreTable[] = {
    // rock -- the bulk of what a pick turns up near the surface
    {MAT_PUMICE, 0, 30, 10, 5},
    {MAT_STONE, 0, 30, 20, 10},
    {MAT_GRANITE, 0, 15, 25, 10},
    {MAT_MARBLE, 3, 5, 10, 5},
    {MAT_OBSIDIAN, 5, 2, 8, 10},
    {MAT_JADE, 7, 1, 3, 6},
    {MAT_MALACHITE, 5, 2, 4, 6},

    // metal -- the mountains' business
    {MAT_TIN, 0, 12, 10, 5},
    {MAT_COPPER, 0, 12, 12, 6},
    {MAT_ALUMINUM, 2, 4, 6, 4},
    {MAT_IRON, 2, 8, 15, 8},
    {MAT_SILVER, 4, 3, 8, 6},
    {MAT_GOLD, 5, 2, 6, 5},
    {MAT_PLATINUM, 7, 1, 3, 3},
    {MAT_TITANIUM, 9, 1, 3, 3},
    {MAT_TUNGSTEN, 9, 1, 2, 3},
    {MAT_MITHRIL, 11, 1, 2, 4},
    {MAT_ADAMANTITE, 13, 1, 1, 3},

    // crystal -- rare in the open, common enough in the deep dark
    {MAT_MICA, 0, 5, 5, 15},
    {MAT_QUARTZ, 0, 5, 8, 20},
    {MAT_ONYX, 5, 2, 4, 12},
    {MAT_OPAL, 7, 1, 3, 10},
    {MAT_CRYSTAL, 9, 2, 3, 10},
    {MAT_AMETHYST, 9, 1, 2, 8},
    {MAT_CORUNDUM, 9, 1, 2, 6},
    {MAT_DIAMOND, 9, 1, 1, 4},
    {MAT_RUBY, 9, 1, 1, 3},
    {MAT_EMERALD, 9, 1, 1, 3},
    {MAT_SAPPHIRE, 9, 1, 1, 3},
  };
}  // namespace

int getOreMaterial(const TRoom* rp) {
  int depth = getMineDepth(rp);
  sectorTypeT sector = rp->getSectorType();
  bool cave = (sector == SECT_ARCTIC_CAVE || sector == SECT_TEMPERATE_CAVE ||
               sector == SECT_TROPICAL_CAVE);
  bool mountain = rp->isMountainSector();

  int total = 0;
  for (const auto& ore : oreTable) {
    if (depth < ore.minDepth)
      continue;
    total += cave ? ore.cave : (mountain ? ore.mountain : ore.hill);
  }

  if (total <= 0)
    return MAT_STONE;

  int roll = ::number(1, total);

  for (const auto& ore : oreTable) {
    if (depth < ore.minDepth)
      continue;

    roll -= cave ? ore.cave : (mountain ? ore.mountain : ore.hill);
    if (roll <= 0)
      return ore.material;
  }

  return MAT_STONE;
}

// Three families come out of a rock face and they are not the same kind of
// thing. Metal is ore, and will be ore until it sees a crucible. Rock is just
// rock, cut loose. A crystal is already what it is going to be -- it only
// needs cutting -- so it is never called ore.
void nameMinedThing(TObj* ore, int material) {
  sstring what = material_nums[material].mat_name;

  switch (getMaterialFamily(material)) {
    case FAM_METAL:
      ore->name = format("ore chunk %s") % what;
      ore->shortDescr = format("a chunk of %s ore") % what;
      ore->setDescr(format("A chunk of %s ore lies here.") % what);
      break;

    case FAM_CRYSTAL:
      ore->name = format("stone uncut rough %s") % what;
      ore->shortDescr = format("an uncut %s") % what;
      ore->setDescr(format("An uncut %s lies here, still rough.") % what);
      break;

    case FAM_ROCK:
    default:
      ore->name = format("block rough %s") % what;
      ore->shortDescr = format("a rough block of %s") % what;
      ore->setDescr(format("A rough block of %s lies here.") % what);
      break;
  }
}

TObj* makeOreChunk(TRoom* rp) {
  if (!rp)
    return nullptr;

  int material = getOreMaterial(rp);
  TObj* ore = read_object(MINE_ORE_VNUM, VIRTUAL);
  if (!ore)
    return nullptr;

  ore->swapToStrung();
  ore->setMaterial(material);
  ore->setWeight(weightForVolume(ore->getVolume(), material));
  ore->obj_flags.decay_time = OBJ_NOTIMER;

  nameMinedThing(ore, material);

  *rp += *ore;

  return ore;
}

// Blast size is the first chance, then eight less, then eight less again, all
// the way down -- every step its own roll, so a big charge usually shakes
// several chunks loose and a small one usually shakes none.
void revealOreFromBlast(TBeing* ch, int trapLevel) {
  if (!ch || !ch->roomp)
    return;

  TRoom* rp = ch->roomp;

  if (!isMineableSector(rp) || rp->getMinedOut())
    return;

  int found = 0;
  for (int chance = trapLevel; chance > 0; chance -= 8)
    if (::number(1, 100) <= chance)
      found++;

  if (!found)
    return;

  for (int i = 0; i < found; i++)
    makeOreChunk(rp);

  act("The blast tears into the rock, and something comes loose.", false, ch, 0,
    0, TO_CHAR);
  act("The blast tears into the rock, and something comes loose.", false, ch, 0,
    0, TO_ROOM);

  // A blast works the face as hard as a day of swinging does, so it answers to
  // the same chance of finishing the room off. Without this a deep cave and a
  // sack of charges would be an endless supply.
  if (::number(1, 100) <= getMinedOutChance(rp)) {
    rp->setMinedOut(1);
    act("What is left of the face slumps and goes barren.", false, ch, 0, 0,
      TO_ROOM);
  }
}

static void mining_pulse(TBeing* ch) {
  TRoom* rp = ch->roomp;
  if (!isMineableSector(rp)) {
    ch->sendTo("There is nothing here worth cutting into.\n\r");
    ch->stopTask();
    return;
  }

  if (rp->getMinedOut()) {
    ch->sendTo("This rock has nothing left in it.\n\r");
    ch->stopTask();
    return;
  }

  if (!getHeldPick(ch)) {
    ch->sendTo("You have nothing to swing.\n\r");
    ch->stopTask();
    return;
  }

  ch->addToMove(-(::number(5, 15)));
  if (ch->getMove() < 10) {
    act("You are far too tired to keep swinging.", false, ch, 0, 0, TO_CHAR);
    act("$n lowers $s pick, breathing hard.", true, ch, 0, 0, TO_ROOM);
    ch->stopTask();
    return;
  }

  if (!ch->bSuccess(SKILL_MINE)) {
    CF(SKILL_MINE);

    if (!::number(0, 2))
      act("Your pick rings off the rock and nothing comes away.", false, ch, 0,
        0, TO_CHAR);

    return;
  }

  CS(SKILL_MINE);

  if (--ch->task->timeLeft > 0) {
    if (!::number(0, 2))
      act("You work another seam loose.", false, ch, 0, 0, TO_CHAR);
    return;
  }

  TObj* ore = makeOreChunk(rp);

  if (!ore) {
    ch->sendTo("The seam crumbles to dust in your hands.\n\r");
    ch->stopTask();
    return;
  }

  // Cut by hand, so it goes straight into the miner's arms rather than onto
  // the floor.
  --(*ore);
  *ch += *ore;

  act("You work $p free of the rock.", false, ch, ore, 0, TO_CHAR);
  act("$n works $p free of the rock.", true, ch, ore, 0, TO_ROOM);

  // Paid the way lumberjack pays: what the miner knows is the base, and the
  // multiplier stays at one. Logging divides that base by the logs already
  // taken from the room; rock keeps no such tally -- it is worked or it is
  // barren -- so there is nothing here to divide by.
  int learning = ch->getSkillValue(SKILL_MINE);
  ch->gainTaskExp(SKILL_MINE, max(1, learning), 1.0, false);

  // Every seam taken is a chance the room is finished. Deep rock gives out
  // slowly; a hillside is worked bare in a few swings.
  if (::number(1, 100) <= getMinedOutChance(rp)) {
    rp->setMinedOut(1);
    ch->sendTo("The seam runs out, and the rock here goes barren.\n\r");
    ch->stopTask();
    return;
  }

  // Otherwise keep going: the next chunk is another stretch of work.
  ch->task->timeLeft = 10;
}

int task_mining(TBeing* ch, cmdTypeT cmd, const char*, int pulse, TRoom*,
  TObj*) {
  if (ch->isLinkdead() || (ch->getPosition() <= POSITION_SITTING)) {
    ch->stopTask();
    return false;
  }

  if (ch->utilityTaskCommand(cmd) || ch->nobrainerTaskCommand(cmd))
    return false;

  if (ch->in_room != ch->task->wasInRoom) {
    ch->sendTo("You wander away from the rock face.\n\r");
    ch->stopTask();
    return false;
  }

  if (!ch->doesKnowSkill(SKILL_MINE)) {
    ch->sendTo("You've forgotten how to do this.\n\r");
    ch->stopTask();
    return false;
  }

  switch (cmd) {
    case CMD_TASK_CONTINUE:
      ch->task->calcNextUpdate(pulse, Pulse::MOBACT);
      mining_pulse(ch);
      return false;
    case CMD_ABORT:
    case CMD_STOP:
      act("You shoulder your pick.", false, ch, 0, 0, TO_CHAR);
      act("$n shoulders $s pick.", true, ch, 0, 0, TO_ROOM);
      ch->stopTask();
      break;
    case CMD_TASK_FIGHTING:
      ch->sendTo("You can't mine while under attack!\n\r");
      ch->stopTask();
      break;
    default:
      if (cmd < MAX_CMD_LIST)
        warn_busy(ch);
      break;
  }

  return true;
}

void TBeing::doMine(const char*) {
  if (!doesKnowSkill(SKILL_MINE)) {
    sendTo("You know nothing about working rock.\n\r");
    return;
  }

  if (!isMineableSector(roomp)) {
    sendTo("There is no rock here worth cutting into.\n\r");
    return;
  }

  if (roomp->getMinedOut()) {
    sendTo("This rock has already been worked out.\n\r");
    return;
  }

  if (!getHeldPick(this)) {
    sendTo("You need a pick in hand to mine.\n\r");
    return;
  }

  if (task)
    stopTask();

  act("You set your pick against the rock and begin.", false, this, 0, 0,
    TO_CHAR);
  act("$n sets $s pick against the rock.", true, this, 0, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_MINE, 8);

  start_task(this, nullptr, roomp, TASK_MINING, "", 10, in_room, 0, 0, 0);
}
