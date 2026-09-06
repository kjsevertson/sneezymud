//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
// augment.h - shared pieces of the gear augmentation system
//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "obj.h"
#include "obj_low.h"
#include "race.h"
#include "wearTemplate.h"

class TBeing;
class TBaseClothing;
class TCommodity;

// AC and structure both derive from one number: the level of the mob that
// loads the gear. Each armor tier sits at a fixed point on that scale, and a
// tier move is a rescale between two of these -- see the tier table in
// docs/superpowers/specs/2026-08-23-gear-augmentation-design.md.
//
// Demotion rescales the item's own level rather than snapping it to the tier's
// number, so a piece that loaded off a level 70 mob keeps what made it good.
// The skill ceiling in that table caps the skills that *add* value; Strip only
// ever subtracts, so it needs no cap.
[[nodiscard]] constexpr double getTierLoadLevel(Tier tier) {
  switch (tier) {
    case Tier_Heavy:
      return 60.0;
    case Tier_Medium:
      return 50.0;
    case Tier_Light:
      return 40.0;
    case Tier_Clothing:
      return 30.0;
    default:
      return 0.0;
  }
}

// Skills that add value do not reach what the world loads: heavy tops out at
// level 55 rather than 60, and every other tier scales by the same factor. The
// factor is uniform across the ladder on purpose -- the base levels above
// already carry the tier spread, and applying it twice would compound hardest
// on the bottom rung, where it is least wanted.
inline constexpr double kSkillCeilingFactor = 55.0 / 60.0;

[[nodiscard]] constexpr double getTierSkillMax(Tier tier) {
  return getTierLoadLevel(tier) * kSkillCeilingFactor;
}

// One rung up the armor ladder, or Tier_Max at the top.
[[nodiscard]] constexpr Tier getTierAbove(Tier tier) {
  switch (tier) {
    case Tier_Clothing:
      return Tier_Light;
    case Tier_Light:
      return Tier_Medium;
    case Tier_Medium:
      return Tier_Heavy;
    default:
      return Tier_Max;
  }
}

// The highest tier a material will hold, from material_nums[].hardness. This
// is a cap, not a fit: material says how high an item can climb, and Transmute
// is what raises it.
[[nodiscard]] Tier getMaxTierForMaterial(unsigned short material);

// One rung down the armor ladder, or Tier_Max at the bottom. Jewelry is not on
// the ladder -- it is Bangle's output, not a rung.
[[nodiscard]] constexpr Tier getTierBelow(Tier tier) {
  switch (tier) {
    case Tier_Heavy:
      return Tier_Medium;
    case Tier_Medium:
      return Tier_Light;
    case Tier_Light:
      return Tier_Clothing;
    default:
      return Tier_Max;
  }
}

// The anti-class flags that hold an item at this tier rather than the one
// below: clearing them is what a demotion physically does. Mirrors the mask
// layering in ArmorEvaluator::getTier().
[[nodiscard]] unsigned int getTierRungFlags(Tier tier);

// The item's tier as the evaluator sees it, including the restrictions that
// getTier() infers rather than reads off flags.
[[nodiscard]] Tier getWearableTier(const TBaseClothing* clothing);

// The item's wear slot as a template slot, or TemplateSlot::COUNT if it has
// none, or more than one.
[[nodiscard]] TemplateSlot getWearableSlot(const TObj* obj);

// Rebuild a wearable as a different item type, carrying its state across, and
// hand it back in place of the original in ch's inventory. The original is
// deleted. Returns nullptr, original intact, if there is no template for the
// item's slot.
//
// C++ has no in-place type change, and rent rebuilds an item's class and
// wear_flags from the prototype at its vnum, so the new item must come from a
// real template vnum rather than a bare constructor.
[[nodiscard]] TObj* convertWearableType(TBeing* ch, TObj* obj, itemTypeT type);

// The cloth-family materials Sew will work: everything soft enough to sit
// under the clothing hardness threshold. Leather reads 20 and belongs to
// Light, so it is not on the list.
[[nodiscard]] bool isSewableMaterial(unsigned short material);

// Slot named on a command line ("wrist", "head"), or TemplateSlot::COUNT.
[[nodiscard]] TemplateSlot getTemplateSlotFromName(const sstring& name);

// Race named on a command line, or RACE_NORACE if it is not one.
[[nodiscard]] race_t getRaceFromName(const sstring& name);

// Material named on a command line, or -1.
[[nodiscard]] int getMaterialFromName(const sstring& name);

// Units of commodity a woven piece costs. Commodities carry ten units per
// point of weight, so the piece's own weight converts directly; a clumsy
// hand spoils more of the thread.
[[nodiscard]] int getSewUnits(const TBeing* ch, float weight);

// The level a woven piece comes out at: the clothing ceiling, scaled by the
// sewer's level up to 35 and by how well they know the skill.
[[nodiscard]] double getSewLevelMax(const TBeing* ch);

// Find a carried commodity of this material.
[[nodiscard]] TCommodity* findCommodity(TBeing* ch, unsigned short material);

// Spend units of a carried commodity. False if it is no longer there -- it can
// be dropped or sold while the task is still running.
bool consumeCommodity(TBeing* ch, unsigned short material, int units);

// Metals run 150 through 177 in materials.h (MAX_MAT_METAL = 28, counted from
// 150). Smelt cares about nothing else on an item: a steel key and a mithril
// breastplate are both metal, and both go in the crucible.
[[nodiscard]] bool isMetalMaterial(unsigned short material);

// The vnum of the ingot prototype, created by a migration. Rent rebuilds an
// item's class from its vnum, so a smelted ingot has to come from a real
// prototype rather than a bare constructor -- but one row covers every metal,
// since material, quality and units are all persisted state.
inline constexpr int kIngotVnum = 29539;

// A smelt's two tallies, packed into the task's flags word: landed pulses in
// the low half, missed ones in the high half. The ratio between them is the
// grade of the ingot that comes out.
[[nodiscard]] inline int smeltHits(int flags) { return flags & 0xFFFF; }
[[nodiscard]] inline int smeltMisses(int flags) { return (flags >> 16) & 0xFFFF; }
[[nodiscard]] inline int smeltTally(int hits, int misses) {
  return (min(misses, 0xFFFF) << 16) | min(hits, 0xFFFF);
}

// Grade from the ratio of landed pulses to total: 90% earns a 5, 80% a 4, 65%
// a 3, 45% a 2, and anything below that a 1.
[[nodiscard]] int getSmeltQuality(int hits, int misses);

// How much of a smelted item's weight comes back as metal: half, or three
// quarters for a smith with real standing in the advanced blacksmithing
// discipline.
[[nodiscard]] int getSmeltUnits(const TBeing* ch, float weight);

// The metal tables in materials.h carry curated per-material numbers that the
// smithing skills read instead of deriving their own: difficultyMod is
// documented as subtracted from the skill check, and structureMod is the
// material's contribution to how much work a piece of it is. Metals absent
// from that table fall back to hardness, which is the only thing every
// material has.
[[nodiscard]] int getMetalDifficulty(unsigned short material);
[[nodiscard]] int getMetalStructure(unsigned short material);

// The modifier a smithing roll carries: the metal resists, and depth in
// advanced blacksmithing buys it back at a point per five learned.
[[nodiscard]] int getForgeRollMod(const TBeing* ch, unsigned short material);

// How much work a bar of this metal and size is. A hundred units of a metal
// costs exactly its structureMod, and it scales from there -- a bigger bar is
// a longer job, which is the whole reason size lives here and not in the roll.
[[nodiscard]] int getIngotStructure(unsigned short material, int units);

// True if any affect on the item does something. Only a blank bar may be fed
// into another: combining two stat-carrying ingots would have to merge two
// sets of affects, and there is no rule for that.
[[nodiscard]] bool hasApplies(const TObj* obj);

// Quality of the bar that comes out of a combine: the two grades averaged by
// size, rounded down so improvement has to be earned, and never below what the
// first bar already was. Scrap cannot spoil good metal; good metal can lift
// scrap.
[[nodiscard]] int getCombineQuality(int q1, int u1, int q2, int u2);

// Charge a pulse of physical work against movement, the way repair does.
// Returns true when the worker is spent and the task should stop; the messages
// are sent from here.
//
// heavy marks the metal work -- smelting, forging, plating, drawing down --
// which costs what MetalRepair costs and carries its dwarf bonus. Cloth work
// runs at the lighter rate wood repair uses. Skill buys back part of the
// drain, and working on home turf or in the terrain someone grew up in buys
// back a point each, the same two conditions specialAttack rewards.
[[nodiscard]] bool augmentDrain(TBeing* ch, spellNumT skill, bool heavy);

// How much of the structure clock one successful pulse burns, for the skill
// given. A master cuts three points per landed pulse, a novice one.
[[nodiscard]] int getAugmentTickAmount(const TBeing* ch, spellNumT skill);

// Tier named on a command line ("heavy", "light"), or Tier_Max.
[[nodiscard]] Tier getTierFromName(const sstring& name);

// Every anti-class flag an item at this tier carries: the rungs are
// cumulative, so a heavy piece wears all six.
[[nodiscard]] unsigned int getTierFlags(Tier tier);

// The level a forged piece is projected at before any of it is lost to bad
// work: the tier's ceiling, scaled by the smith's level up to 35 and by how
// well they know the skill. Sew's formula, with the tier's own ceiling.
[[nodiscard]] double getForgeLevelMax(const TBeing* ch, Tier tier);

// Knock a point off one surviving stat on the piece, chosen at random. This is
// what a missed forge pulse costs: the work is never wasted, it just comes out
// carrying less of what the metal remembered.
void reduceOneApply(TObj* obj);

// Apply the tier move. Called once, when the clock runs out.
void stripFinish(TBeing* ch, TObj* obj);
void plateFinish(TBeing* ch, TObj* obj);
void sewFinish(TBeing* ch, TObj* obj);
void bangleFinish(TBeing* ch, TObj* obj);

// Smelt hands its tallies in: the object is gone by the time it returns.
void smeltFinish(TBeing* ch, TObj* obj, int hits, int misses);

// Combine folds the donor bar into the target and destroys it. The tallies are
// the combine task's own: workmanship caps how much the grade can improve.
void combineFinish(TBeing* ch, TObj* target, TObj* donor, int hits,
  int misses);

// A missed forge pulse spoils metal still on the bench, not just the piece on
// the anvil. Removes units from the named bar, destroying it if that empties
// it; does nothing if the bar is already gone.
void spoilLeftoverMetal(TBeing* ch, const char* ingotName, int amount);

// The same for thread: a botched stitch wastes what is still on the skein.
void spoilLeftoverThread(TBeing* ch, const char* skeinName, int amount);

// The soulstone prototype, created by a migration. As with the ingot, one row
// covers every stone: Level and charges are persisted state.
inline constexpr int kSoulstoneVnum = 29540;

// Five carats to a Soulstone Level, clamped to 1-10.
[[nodiscard]] int getSoulLevelForCarats(int carats);

// Ten landed pulses per carat of the opal being worked.
[[nodiscard]] int getEnsoulLength(int carats);

// Ensoul's finish: the opal is destroyed and a soulstone takes its place.
void ensoulFinish(TBeing* ch, TObj* obj, int carats);

// A corpse nothing has been taken out of yet. Rites needs one whole: the soul
// is the last thing to leave, and a body that has been skinned, butchered or
// dissected has already been opened.
[[nodiscard]] bool corpseIsPristine(const TObj* corpse);

// Holy water carried, in units. Rites spends it the way Attune does.
[[nodiscard]] class TVial* findHolyWater(TBeing* ch);

// The first soulstone in inventory, or nullptr.
[[nodiscard]] class TSoulstone* findSoulstone(TBeing* ch);

// What one Rites yields: the stone's Level against a tenth of the corpse's.
// Skill decides whether the ritual lands at all, and never how much -- the
// failure rate is limiter enough, and the caster's own level never enters.
[[nodiscard]] int getRitesYield(int soulLevel, int corpseLevel);

// Rites' finish: charges into the stone, and the corpse closed to everything
// that would have taken something else out of it.
void ritesFinish(TBeing* ch, TObj* corpse);

// How high this character can carry a piece of this tier: the tier's ceiling,
// opening fully at level 35. Bolster tops out at Heavy's 55 and no higher,
// because that is Heavy's ceiling.
[[nodiscard]] double getBolsterMax(const TBeing* ch, Tier tier);

// The roll modifier for a piece this far along toward its ceiling. Difficulty
// is progress, not absolute level: a clothing piece at 22 is four fifths of
// the way to everything it can be and fights hard, while light armor at the
// same 22 is only three fifths along and comes easily. Re-read every attempt,
// so the work gets harder as the piece improves.
[[nodiscard]] int getBolsterBandMod(double armorLevel, double ceiling);

// The band's name, for telling the player what they are up against.
[[nodiscard]] sstring getBolsterBandName(double armorLevel, double ceiling);

// Charges a single attempt costs, won or lost. Soulstone Level divides it, so
// a Level 10 stone does the same work for a tenth of what a Level 1 pays.
[[nodiscard]] int getBolsterChargeCost(double armorLevel, int soulLevel);

// The essence prototype, created by a migration.
inline constexpr int kEssenceVnum = 29541;

// What one affect deposits: its magnitude, scaled by how well the mage knows
// the work, never less than 1. Learnedness scales the yield here, unlike in
// Rites where it only gates success -- the two are deliberately different.
[[nodiscard]] int getDistillDeposit(const TBeing* ch, int modifier);

// An essence of this apply already in hand, or nullptr. Distill adds to one
// rather than minting a second, so repeated work accumulates toward the next
// Quality instead of scattering across duplicates.
[[nodiscard]] class TEssence* findEssence(TBeing* ch, int apply);

// Put charges of an apply into the mage's essences, minting a Quality 1
// essence if none of that apply is held. Returns the essence written to.
class TEssence* depositEssence(TBeing* ch, int apply, int charges);

// Distill's finish: every eligible affect on the item deposits, and the item
// is destroyed.
void distillFinish(TBeing* ch, TObj* obj);

// The twelve applies that count as stats for Infuse's purposes: the six
// primary and six secondary. Pools, combat and perception applies are not
// among them and do not count against the limit.
[[nodiscard]] bool isStatApply(int apply);

// True for the three pools, which are written in blocks of five: a point of
// mana is not worth the same as a point of strength, so an essence of one
// writes five times what an essence of the other does.
[[nodiscard]] bool isPoolApply(int apply);

// Where this apply stands among the stats already on the item: 1 for the
// first, 2 for the second, 3 for the third, 4 or more for one that cannot be
// added. An apply already present keeps its own place in the order.
[[nodiscard]] int getStatRank(const TObj* obj, int apply);

// The most this apply may be raised to on this item. The first stat on a piece
// may reach 5, the second 4, the third 3 -- so a piece specialised in one
// thing beats a piece spread across three.
[[nodiscard]] int getInfuseMax(const TObj* obj, int apply);

// What an essence of this quality writes for this apply: the quality itself
// for a stat, five times it for a pool.
[[nodiscard]] int getInfuseAmount(int apply, int quality);

// Infuse's finish: the apply is written and the essence reset to quality 1.
void infuseFinish(TBeing* ch, TObj* obj, const char* essenceName);

// The ten material families, ordered so that neighbours are things one could
// imagine becoming each other and the ends are absurd. Transmute charges by
// how far it has to travel along this line, so metal to crystal is a short
// step and foodstuff to starmetal is the length of the world.
enum MaterialFamily {
  FAM_METAL = 0,
  FAM_CRYSTAL,
  FAM_ROCK,
  FAM_DEAD,
  FAM_GENERIC,
  FAM_ORGANIC,
  FAM_HIDE,
  FAM_WOOD,
  FAM_MAGICAL,
  FAM_SPIRITUAL,
  FAM_NONE
};

[[nodiscard]] MaterialFamily getMaterialFamily(unsigned short material);

// The rarity cost the material's own family table carries: -10 for common,
// +10 uncommon, +25 rare, +40 legendary. The same scale in every family, which
// is what makes distances between them comparable.
[[nodiscard]] int getMaterialRarity(unsigned short material);

// What a transmutation costs on the roll: ten per family crossed, plus the
// target's rarity. Working inside one family gives back half of what the
// starting material was worth, so beginning from something already fine makes
// a fine result reachable.
[[nodiscard]] int getTransmuteRollMod(unsigned short from, unsigned short to);

// The share of a transmutation's pulses that must land for the change to take.
// Below it the work simply fails, and the opal is spent either way.
inline constexpr int kTransmuteSuccessRatio = 65;

// How long a transmutation runs: the distance between the two materials, plus
// what the target is worth, plus the weight of the thing being changed. Heavy
// pieces and far reaches both take longer.
[[nodiscard]] int getTransmuteLength(unsigned short from, unsigned short to,
  float weight);

// The weapon template, created by a migration. One row serves all eighty.
inline constexpr int kWeaponVnum = 29544;

// The damage level a smith can forge into a new weapon: six sevenths of their
// level, and the level stops counting at 35. A master forges at 30 and gets no
// further at the anvil -- everything above that has to be honed in.
[[nodiscard]] int getForgeWeaponMax(const TBeing* ch);

// The ceiling honing can reach: the smith's own level plus a point for every
// twenty they know of the skill.
[[nodiscard]] int getHoneMax(const TBeing* ch);

// A weapon is only as fine as the hand that makes it: the sharper the kind of
// weapon, the more of the skill it takes to attempt at all.
[[nodiscard]] bool canForgeWeapon(const TBeing* ch, int maxSharp);

// Transmute's finish: the item becomes the new material if enough of the work
// landed, and its weight follows -- volume is the thing that stays.
void transmuteFinish(TBeing* ch, TObj* obj, unsigned short material, int hits,
  int misses);

// The offcut prototype, created by a migration. Metal cut off a piece when it
// is resized down: junk until it goes back through the crucible.
inline constexpr int kOffcutVnum = 29542;

// Copy the stat affects from one item to another, leaving AC behind. AC
// belongs to the shape of a thing and is owned by the ladder and Bolster; the
// stats are what survive being cut apart.
void copyStatApplies(const TObj* from, TObj* to);

// Cut material away from a piece and hand back what came off: an offcut of
// the same material at the leftover volume, carrying the piece's stats but not
// its AC. Junk on its own -- metal has to go back through the crucible, cloth
// through a mage -- which is what keeps resizing a step rather than a
// shortcut. Returns nullptr if there was nothing left over.
class TObj* makeOffcut(TBeing* ch, TObj* obj, int leftover);

// True if this item is an offcut. Distill reads them as a source of essence
// and Transmute can turn one into another material, so the two of them are
// what an offcut is for.
[[nodiscard]] bool isOffcut(const TObj* obj);

// Resize's finish: the piece is remade at the new size, and material cut away
// becomes an offcut carrying what the piece carried.
void resizeFinish(TBeing* ch, TObj* obj, race_t race);

// The skein prototype, created by a migration.
inline constexpr int kSkeinVnum = 29543;

// Soft materials: everything in the hide table, which is what Weave will pull
// apart and Sew will build from. Difficulty comes from that table's rarity
// tiers; how high a piece made of it can climb comes from hardness. The two
// disagree on purpose -- silk is legendary and stays clothing, leather is
// common and reaches light.
[[nodiscard]] bool isSoftMaterial(unsigned short material);
[[nodiscard]] int getHideDifficulty(unsigned short material);
[[nodiscard]] int getHideStructure(unsigned short material);

// The modifier a weaving or sewing roll carries against this material.
[[nodiscard]] int getFibreRollMod(const TBeing* ch, unsigned short material);

// How much work a skein of this fibre and size is, the clock for anything done
// to it.
[[nodiscard]] int getSkeinStructure(unsigned short material, int units);

// Weave's finish: the worn thing is pulled apart and its fibre comes back as a
// skein carrying what it carried.
void weaveFinish(TBeing* ch, TObj* obj, int hits, int misses);

// Tailor's finish: the cloth twin, leaving clippings rather than metal.
void tailorFinish(TBeing* ch, TObj* obj, race_t race);

// Forge's finish: the projected level, less what the misses cost, written onto
// the piece. penalty is a percentage.
void forgeFinish(TBeing* ch, TObj* obj, int penalty);

// Jewelry's half of the building convention: it carries half the AC and
// structure a wearable of the same level and slot would, in exchange for twice
// the stat allowance. Applied to the values themselves, not to the level --
// setDefArmorLevel() carries a fixed newbie-gear offset, so half a level is
// nowhere near half an AC.
void halveArmorValues(TBaseClothing* clothing);

// Every anti-class flag, which jewelry carries none of.
[[nodiscard]] unsigned int allTierFlags();
