//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD++ - All rights reserved, SneezyMUD Coding Team
//
//      "bulkLoadOut.cc" - Random equipment generation for humanoid mobs
//
//////////////////////////////////////////////////////////////////////////

#include <array>
#include <cmath>
#include <vector>

#include "being.h"
#include "database.h"
#include "discipline.h"
#include "extern.h"
#include "immunity.h"
#include "materials.h"
#include "monster.h"
#include "obj_base_clothing.h"
#include "obj_commodity.h"
#include "obj_general_weapon.h"
#include "bulkLoadOut.h"
#include "race.h"
#include "shop.h"
#include "shopowned.h"
#include "spells.h"
#include "stats.h"
#include "wearTemplate.h"

namespace {

// -----------------------------------------------------------------------
// Material density table (g/cm^3, real-world values)
// Indexed by MAT_* constants from materials.h
// Used to derive item weight from volume: weight = volume * density * k
// where k = 0.0002716 (calibrated so full human steel armor = 90 lbs)
// -----------------------------------------------------------------------
inline constexpr double WEIGHT_CONSTANT = 0.0002716;

inline constexpr double material_density[200] = {
    // 0-9
    1.0,   // MAT_UNDEFINED
    0.07,  // MAT_PAPER
    0.15,  // MAT_CLOTH
    0.9,   // MAT_WAX
    2.5,   // MAT_GLASS
    0.6,   // MAT_WOOD
    0.13,  // MAT_SILK
    0.9,   // MAT_FOODSTUFF
    1.1,   // MAT_PLASTIC
    1.2,   // MAT_RUBBER

    // 10-19
    0.1,   // MAT_CARDBOARD
    0.12,  // MAT_STRING
    0.01,  // MAT_PLASMA
    0.2,   // MAT_TOUGHENED_CLOTH
    2.0,   // MAT_CORAL
    0.10,  // MAT_HORSEHAIR
    0.08,  // MAT_HAIR
    0.05,  // MAT_ASH
    0.5,   // MAT_PUMICE
    0.4,   // MAT_LAMINATE

    // 20-49: empty slots
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 20-29
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 30-39
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 40-49

    // 50-59
    0.8,   // MAT_GEN_ORGANIC
    0.9,   // MAT_LEATHER
    1.0,   // MAT_TOUGHENED_LEATHER
    2.5,   // MAT_DRAGON_SCALE
    0.18,  // MAT_WOOL
    0.20,  // MAT_FUR
    0.05,  // MAT_FEATHERED
    1.0,   // MAT_LIQUID
    0.01,  // MAT_FIRE
    2.2,   // MAT_EARTH

    // 60-69
    1.0,   // MAT_GEN_ELEMENT
    0.92,  // MAT_ICE
    0.01,  // MAT_LIGHTNING
    0.5,   // MAT_CHAOS
    1.8,   // MAT_CLAY
    2.4,   // MAT_PORCELAIN
    0.08,  // MAT_STRAW
    2.7,   // MAT_PEARL
    1.0,   // MAT_FLESH
    0.12,  // MAT_FUR_CAT

    // 70-79
    0.12,  // MAT_FUR_DOG
    0.10,  // MAT_FUR_RABBIT
    0.01,  // MAT_GHOSTLY
    1.0,   // MAT_DWARVEN_LEATHER
    0.85,  // MAT_SOFT_LEATHER
    1.5,   // MAT_FISHSCALE
    1.1,   // MAT_OGRE_HIDE
    0.25,  // MAT_HEMP
    0, 0,  // 78-79

    // 80-99: empty slots
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 80-89
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 90-99

    // 100-109: gems/stones
    3.5,   // MAT_GEN_MINERAL
    0,     // 101 (MAT_JEWELED is commented out in materials.h)
    2.6,   // MAT_RUNESTONE
    2.6,   // MAT_CRYSTAL
    3.5,   // MAT_DIAMOND
    1.1,   // MAT_EBONY
    2.7,   // MAT_EMERALD
    1.8,   // MAT_IVORY
    2.4,   // MAT_OBSIDIAN
    2.6,   // MAT_ONYX

    // 110-119
    2.1,   // MAT_OPAL
    4.0,   // MAT_RUBY
    4.0,   // MAT_SAPPHIRE
    2.7,   // MAT_MARBLE
    2.6,   // MAT_STONE
    1.9,   // MAT_BONE
    3.3,   // MAT_JADE
    1.1,   // MAT_AMBER
    2.7,   // MAT_TURQUOISE
    2.65,  // MAT_AMETHYST

    // 120-129
    2.8,   // MAT_MICA
    2.2,   // MAT_DRAGONBONE
    3.8,   // MAT_MALACHITE
    2.7,   // MAT_GRANITE
    2.65,  // MAT_QUARTZ
    1.3,   // MAT_JET
    4.0,   // MAT_CORUNDUM
    0, 0, 0,  // 127-129

    // 130-149: empty slots
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 130-139
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 140-149

    // 150-159: metals
    7.0,   // MAT_GEN_METAL
    8.96,  // MAT_COPPER
    0,     // MAT_SCALE_MAIL
    0,     // MAT_BANDED_MAIL
    0,     // MAT_CHAIN_MAIL
    0,     // MAT_PLATE
    8.8,   // MAT_BRONZE
    8.5,   // MAT_BRASS
    7.87,  // MAT_IRON
    7.8,   // MAT_STEEL

    // 160-169
    3.5,   // MAT_MITHRIL
    8.0,   // MAT_ADAMANTITE
    10.49, // MAT_SILVER
    19.3,  // MAT_GOLD
    21.45, // MAT_PLATINUM
    4.5,   // MAT_TITANIUM
    2.7,   // MAT_ALUMINUM
    0,     // MAT_RINGMAIL
    8.9,   // MAT_GNICKEL
    12.5,  // MAT_ELECTRUM

    // 170-179
    7.0,   // MAT_ATHANOR
    7.3,   // MAT_TIN
    19.25, // MAT_TUNGSTEN
    7.5,   // MAT_STARMETAL
    8.23,  // MAT_TERBIUM
    0,     // MAT_ELVENMAIL
    0,     // MAT_ELVENSTEEL
    9.0,   // MAT_ETERNIUM
    0,     // 178
    0,     // 179

    // 180-199: empty slots
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 180-189
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 190-199
};

// The table is positional, so one stray placeholder silently shifts every
// entry after it - which is exactly what had happened to the metals above.
// Pin the materials bulk loot can actually select (see statMaterials) plus the
// block boundaries, so an edit that slips an index fails to compile instead of
// quietly making mithril weigh as much as steel.
static_assert(material_density[MAT_GEN_MINERAL] == 3.5);
static_assert(material_density[MAT_COPPER] == 8.96);
static_assert(material_density[MAT_BRONZE] == 8.8);
static_assert(material_density[MAT_BRASS] == 8.5);
static_assert(material_density[MAT_IRON] == 7.87);
static_assert(material_density[MAT_STEEL] == 7.8);
static_assert(material_density[MAT_MITHRIL] == 3.5);
static_assert(material_density[MAT_SILVER] == 10.49);
static_assert(material_density[MAT_GOLD] == 19.3);
static_assert(material_density[MAT_PLATINUM] == 21.45);
static_assert(material_density[MAT_TITANIUM] == 4.5);
static_assert(material_density[MAT_ALUMINUM] == 2.7);
static_assert(material_density[MAT_ELECTRUM] == 12.5);
static_assert(material_density[MAT_TIN] == 7.3);
static_assert(material_density[MAT_ETERNIUM] == 9.0);

// -----------------------------------------------------------------------
// Quality tiers (determined by mob level)
// -----------------------------------------------------------------------
enum class Quality {
  Flimsy,   // 1-19
  Dented,   // 20-29
  Default,  // 30-39
  Honed,    // 40-49
  Superior, // 50-59
  Master,   // 60+
  COUNT
};

struct QualityInfo {
  const char* armorName;    // nullptr = omit from name
  const char* clothName;    // nullptr = omit from name
  int statBonus;
  int skillBonus;           // 0 = no skill bonus (low tiers only)
  int secondaryBonus;       // 0 = no secondary bonus
  double structPct;         // fraction of max struct points
};

inline constexpr std::array<QualityInfo, static_cast<int>(Quality::COUNT)>
    qualityTable = {{
        {"flimsy", "cheap", 1, 5, 0, 0.40},
        {"dented", "patched", 2, 5, 0, 0.60},
        {nullptr, nullptr, 3, 0, 3, 0.80},
        {"honed", "well-made", 4, 0, 5, 1.00},
        {"superior", "excellent", 5, 0, 7, 1.00},
        {"masterwork", "superb", 6, 0, 9, 1.00},
    }};

[[nodiscard]] Quality qualityFromLevel(int level) {
  if (level < 20) return Quality::Flimsy;
  if (level < 30) return Quality::Dented;
  if (level < 40) return Quality::Default;
  if (level < 50) return Quality::Honed;
  if (level < 60) return Quality::Superior;
  return Quality::Master;
}

// -----------------------------------------------------------------------
// Armor tiers — determines material table (armor vs clothing) and
// anti-class flags
// -----------------------------------------------------------------------
enum class ArmorTier { Heavy, Medium, Light, Clothing };

struct ArmorTierInfo {
  ArmorTier tier;
  bool usesArmorMaterials;
  unsigned int antiFlags;
  double acScale;  // AC level = mob level * acScale
};

inline constexpr auto ANTI_HEAVY =
    ITEM_ANTI_CLERIC | ITEM_ANTI_THIEF | ITEM_ANTI_MAGE | ITEM_ANTI_SHAMAN |
    ITEM_ANTI_MONK;
inline constexpr auto ANTI_MEDIUM =
    ITEM_ANTI_THIEF | ITEM_ANTI_MAGE | ITEM_ANTI_SHAMAN | ITEM_ANTI_MONK;
inline constexpr auto ANTI_LIGHT = ITEM_ANTI_SHAMAN | ITEM_ANTI_MONK;

// Indexed by classIndT
inline constexpr std::array<ArmorTierInfo, MAX_CLASSES> armorTierByClass = {{
    {ArmorTier::Light, false, ANTI_LIGHT, 0.57},     // MAGE
    {ArmorTier::Medium, true, ANTI_MEDIUM, 0.71},    // CLERIC
    {ArmorTier::Heavy, true, ANTI_HEAVY, 0.86},      // WARRIOR
    {ArmorTier::Light, false, ANTI_LIGHT, 0.57},     // THIEF
    {ArmorTier::Clothing, false, 0, 0.43},           // SHAMAN
    {ArmorTier::Heavy, true, ANTI_HEAVY, 0.86},      // DEIKHAN
    {ArmorTier::Clothing, false, 0, 0.43},           // MONK
    {},                                                  // RANGER (dead class)
    {},                                                  // COMMONER (dead class)
}};

// -----------------------------------------------------------------------
// Stat → material mapping
// Index corresponds to statTypeT (STR=0 through SPE=11)
// -----------------------------------------------------------------------
inline constexpr int BULK_STAT_COUNT = 12; // STR through SPE

struct StatMaterialInfo {
  int armorMat;        // MAT_* for armor-type items
  int clothMat;        // MAT_* for clothing-type items
  applyTypeT apply;    // APPLY_* for this stat
};

inline constexpr std::array<StatMaterialInfo, BULK_STAT_COUNT> statMaterials = {{
    {MAT_STEEL, MAT_FUR, APPLY_STR},         // STR
    {MAT_IRON, MAT_HORSEHAIR, APPLY_BRA},    // BRA
    {MAT_COPPER, MAT_WOOL, APPLY_CON},       // CON
    {MAT_MITHRIL, MAT_SILK, APPLY_DEX},      // DEX
    {MAT_ELECTRUM, MAT_CLOTH, APPLY_AGI},    // AGI
    {MAT_BRASS, MAT_FEATHERED, APPLY_INT},   // INT
    {MAT_PLATINUM, MAT_HAIR, APPLY_WIS},     // WIS
    {MAT_SILVER, MAT_HEMP, APPLY_FOC},       // FOC
    {MAT_TIN, MAT_FUR_DOG, APPLY_PER},       // PER
    {MAT_GOLD, MAT_FUR_CAT, APPLY_CHA},      // CHA
    {MAT_ALUMINUM, MAT_FUR_RABBIT, APPLY_KAR}, // KAR
    {MAT_TITANIUM, MAT_RUBBER, APPLY_SPE},   // SPE
}};

// -----------------------------------------------------------------------
// Class → primary stats (3 per class, get 3x weight in random selection)
// Indexed by classIndT
// -----------------------------------------------------------------------
struct ClassPrimaryStats {
  std::array<statTypeT, 3> stats;
};

inline constexpr std::array<ClassPrimaryStats, MAX_CLASSES> classPrimaryStats =
    {{
        {{STAT_INT, STAT_FOC, STAT_CHA}}, // MAGE
        {{STAT_WIS, STAT_CON, STAT_PER}}, // CLERIC
        {{STAT_STR, STAT_BRA, STAT_AGI}}, // WARRIOR
        {{STAT_DEX, STAT_FOC, STAT_SPE}}, // THIEF
        {{STAT_INT, STAT_FOC, STAT_CHA}}, // SHAMAN
        {{STAT_BRA, STAT_CON, STAT_WIS}}, // DEIKHAN
        {{STAT_DEX, STAT_SPE, STAT_FOC}}, // MONK
        {},                                // RANGER (dead class)
        {},                                // COMMONER (dead class)
    }};

// -----------------------------------------------------------------------
// Class → base object name per slot
// Indexed by classIndT, then TemplateSlot
// -----------------------------------------------------------------------
inline constexpr auto CS = static_cast<int>(TemplateSlot::COUNT);

using SlotNames = std::array<const char*, CS>;

inline constexpr std::array<SlotNames, MAX_CLASSES> classSlotNames = {{
    // MAGE
    {{"skullcap", "scarf", "robe", "cape", "sleeve", "bracelet", "glove",
      "cord", "trousers", "slipper", "shield", "ring"}},
    // CLERIC
    {{"helmet", "collar", "breastplate", "mantle", "brassard", "maniple",
      "mitten", "skirt", "shinguard", "boot", "shield", "ring"}},
    // WARRIOR
    {{"spangenhelm", "gorget", "brigandine", "backplate", "pauldron",
      "vambrace", "cestus", "fauld", "cuisse", "sabaton", "shield", "ring"}},
    // THIEF
    {{"hood", "cravat", "vest", "cloak", "armband", "cuff", "half-glove",
      "belt", "legging", "shoe", "shield", "ring"}},
    // SHAMAN
    {{"headdress", "choker", "tunic", "shawl", "armlet", "bangle",
      "hand-wrap", "loincloth", "leg-wrap", "sandal", "shield", "ring"}},
    // DEIKHAN
    {{"great helm", "throatguard", "cuirass", "coat", "cannon", "bracer",
      "gauntlet", "girdle", "greave", "jackboot", "shield", "ring"}},
    // MONK
    {{"cowl", "kerchief", "frock", "poncho", "wrap", "wristband", "fistguard",
      "sash", "pants", "sock", "shield", "ring"}},
    {},  // RANGER (dead class)
    {},  // COMMONER (dead class)
}};

// -----------------------------------------------------------------------
// Racial size tiers — derived from newbie starting gear body slot volumes.
// Human (body volume 11000) is the baseline (modifier 1.000).
// Only supported races generate bulk loot; nullptr return = unsupported.
// -----------------------------------------------------------------------
struct RaceSizeInfo {
  const char* name;      // nullptr = omit from name (human-sized)
  double modifier;       // volume multiplier relative to human baseline
  const char* keyword;   // canonical race keyword for this size tier
};

[[nodiscard]] const RaceSizeInfo* raceSizeInfo(race_t race) {
  static constexpr RaceSizeInfo tiny  = {"tiny",  0.470, "hobbit"};
  static constexpr RaceSizeInfo small = {"small", 0.520, "gnome"};
  static constexpr RaceSizeInfo stout = {"stout", 0.630, "dwarf"};
  static constexpr RaceSizeInfo slim  = {"slim",  0.730, "elf"};
  static constexpr RaceSizeInfo def   = {nullptr, 1.000, "human"};
  static constexpr RaceSizeInfo large = {"large", 1.370, "ogre"};

  switch (race) {
    case RACE_HOBBIT:
    case RACE_TROG:    return &tiny;
    case RACE_GNOME:
    case RACE_GOBLIN:  return &small;
    case RACE_DWARF:
    case RACE_FISHMAN: return &stout;
    case RACE_ELVEN:
    case RACE_FROGMAN: return &slim;
    case RACE_HUMAN:
    case RACE_BIRDMAN:
    case RACE_ORC:     return &def;
    case RACE_OGRE:
    case RACE_TROLL:
    case RACE_GNOLL:   return &large;
    default:           return nullptr;
  }
}

// Base volumes per slot at 100% (human) size, derived from newbie gear.
// Actual volume = baseVolume * raceModifier.
inline constexpr std::array<int, TEMPLATE_SLOT_COUNT> slotBaseVolumes = {{
    2500,   // Head
    900,    // Neck (race_vol_constants[neck] * 70)
    11000,  // Body
    2500,   // Back
    2000,   // Arm
    400,    // Wrist
    800,    // Hand
    4000,   // Waist
    6000,   // Leg
    1600,   // Foot
    4500,   // Shield (not race-sized, ~10 lbs in steel)
    0,      // Finger (not race-sized)
}};

// -----------------------------------------------------------------------
// Class → weighted skill pools (for flimsy/dented quality items)
// -----------------------------------------------------------------------
struct ClassSkillPool {
  std::array<spellNumT, 6> skills;
  int count;
};

inline constexpr std::array<ClassSkillPool, MAX_CLASSES> classSkillPools = {{
    // MAGE
    {{SKILL_MANA, SKILL_MEDITATE, SPELL_HANDS_OF_FLAME, SPELL_GRANITE_FISTS,
      SPELL_FLAMING_SWORD, SKILL_WIZARDRY},
     6},
    // CLERIC
    {{SPELL_HEAL_SERIOUS, SPELL_HEAL_CRITICAL, SPELL_FLAMESTRIKE,
      SKILL_PENANCE, SPELL_RAIN_BRIMSTONE, SKILL_DEVOTION},
     6},
    // WARRIOR
    {{SKILL_BASH, SKILL_SLAM, SKILL_BERSERK, SKILL_RESCUE,
      spellNumT(0), spellNumT(0)},
     4},
    // THIEF
    {{SKILL_SNEAK, SKILL_BACKSTAB, SKILL_STABBING, SKILL_TRACK,
      spellNumT(0), spellNumT(0)},
     4},
    // SHAMAN
    {{SKILL_SACRIFICE, SPELL_LIFE_LEECH, SPELL_DISTORT, SPELL_SQUISH,
      SKILL_RITUALISM, spellNumT(0)},
     5},
    // DEIKHAN
    {{SKILL_CHARGE, SKILL_BASH_DEIKHAN, SKILL_RESCUE, SKILL_CHIVALRY,
      spellNumT(0), spellNumT(0)},
     4},
    // MONK
    {{SKILL_YOGINSA, SKILL_KICK, SKILL_CHOP, SKILL_SPRINGLEAP,
      spellNumT(0), spellNumT(0)},
     4},
    {{}, 0},  // RANGER (dead class)
    {{}, 0},  // COMMONER (dead class)
}};

// -----------------------------------------------------------------------
// Secondary bonus types
// -----------------------------------------------------------------------
enum class SecondaryType {
  HP,           // APPLY_HIT
  Mana,         // APPLY_MANA
  Movement,     // APPLY_MOVE
  Vision,       // APPLY_VISION
  Skill,        // APPLY_SPELL (random class-weighted skill)
  Glowing,      // ITEM_GLOW flag (no apply)
  Shadowy,      // ITEM_SHADOWY flag (no apply)
  ImmHeat,      // APPLY_IMMUNITY + IMMUNE_HEAT
  ImmCold,      // APPLY_IMMUNITY + IMMUNE_COLD
  ImmWater,     // APPLY_IMMUNITY + IMMUNE_WATER
  ImmEarth,     // APPLY_IMMUNITY + IMMUNE_EARTH
  ImmAir,       // APPLY_IMMUNITY + IMMUNE_AIR
  ImmLightning, // APPLY_IMMUNITY + IMMUNE_ELECTRICITY
  CritFreq,     // APPLY_CRIT_FREQUENCY
  COUNT
};

inline constexpr int SECONDARY_COUNT = static_cast<int>(SecondaryType::COUNT);

struct SecondaryInfo {
  const char* ansiColor;  // nullptr for flag-based secondaries
  Quality minTier;        // minimum quality tier to appear
};

inline constexpr std::array<SecondaryInfo, SECONDARY_COUNT> secondaryInfo = {{
    {"<r>", Quality::Flimsy},      // HP - red
    {"<c>", Quality::Flimsy},      // Mana - cyan
    {"<G>", Quality::Flimsy},      // Movement - bright green
    {"<y>", Quality::Flimsy},      // Vision - yellow
    {"<p>", Quality::Flimsy},      // Skill - purple
    {nullptr, Quality::Flimsy},    // Glowing - no color
    {nullptr, Quality::Flimsy},    // Shadowy - no color
    {"<R>", Quality::Default},     // ImmHeat - bright red
    {"<C>", Quality::Default},     // ImmCold - bright cyan
    {"<b>", Quality::Default},     // ImmWater - blue
    {"<o>", Quality::Default},     // ImmEarth - orange
    {"<k>", Quality::Default},     // ImmAir - gray
    {"<B>", Quality::Default},     // ImmLightning - bright blue
    {"<P>", Quality::Superior},    // CritFreq - bright purple
}};

// Pick a random secondary type eligible for the given quality tier
[[nodiscard]] SecondaryType pickSecondary(Quality quality) {
  std::vector<SecondaryType> eligible;
  for (int i = 0; i < SECONDARY_COUNT; ++i) {
    if (quality >= secondaryInfo[i].minTier)
      eligible.push_back(static_cast<SecondaryType>(i));
  }
  return eligible[::number(0, static_cast<int>(eligible.size()) - 1)];
}

// -----------------------------------------------------------------------
// Can-be-seen default for all bulk loot items
// -----------------------------------------------------------------------
inline constexpr int BULK_CAN_BE_SEEN = 4;

// -----------------------------------------------------------------------
// Load chance per slot per zone reset (3%)
// -----------------------------------------------------------------------
inline constexpr int BULK_LOAD_CHANCE_PCT = 3;

// -----------------------------------------------------------------------
// Extra description text tables
// -----------------------------------------------------------------------

// Craftsmanship phrase by quality tier (for "It appears to be X")
inline constexpr std::array<const char*, static_cast<int>(Quality::COUNT)>
    craftPhrases = {{
        "poorly-crafted",
        "cheaply-crafted",
        "solidly-crafted",
        "well-crafted",
        "expertly-crafted",
        "masterfully-crafted",
    }};

// Stat → phrase for "makes you think it might {X}"
inline constexpr std::array<const char*, BULK_STAT_COUNT> statPhrases = {{
    "lend you strength",        // STR
    "bolster your brawn",       // BRA
    "improve your constitution",// CON
    "quicken your hands",       // DEX
    "improve your agility",     // AGI
    "sharpen your intellect",   // INT
    "deepen your wisdom",       // WIS
    "sharpen your focus",       // FOC
    "heighten your perception", // PER
    "enhance your charm",       // CHA
    "improve your karma",       // KAR
    "quicken your step",        // SPE
}};

// Format a skill/spell name for use in a description phrase.
[[nodiscard]] sstring skillPhrase(spellNumT skill) {
  if (skill == spellNumT(0) || !discArray[skill])
    return "";
  return format("improve your ability to use %s") % discArray[skill]->name;
}

// Format a secondary bonus type for a description phrase.
[[nodiscard]] sstring secondaryBonusPhrase(SecondaryType sec,
    spellNumT secSkill) {
  switch (sec) {
    case SecondaryType::HP:           return "bolster your health";
    case SecondaryType::Mana:         return "deepen your mana reserves";
    case SecondaryType::Movement:     return "lighten your step";
    case SecondaryType::Vision:       return "sharpen your sight";
    case SecondaryType::Skill:        return skillPhrase(secSkill);
    case SecondaryType::ImmHeat:      return "shield you from heat";
    case SecondaryType::ImmCold:      return "shield you from cold";
    case SecondaryType::ImmWater:     return "shield you from water";
    case SecondaryType::ImmEarth:     return "shield you from earth";
    case SecondaryType::ImmAir:       return "shield you from air";
    case SecondaryType::ImmLightning: return "shield you from lightning";
    case SecondaryType::CritFreq:     return "help you strike true";
    case SecondaryType::Glowing:
    case SecondaryType::Shadowy:
    case SecondaryType::COUNT:        return "";
  }
  return "";
}

// Build the extra description text for a bulk loot item.
// detailWord: "engraving" (armor), "stitching" (clothing), "balance" (weapon)
// category: "armor", "clothing", "weapon"
[[nodiscard]] sstring composeBulkDescription(const char* baseName,
    const char* category, const char* detailWord, classIndT classInd,
    Quality quality, statTypeT stat, spellNumT bonusSkill,
    SecondaryType sec, spellNumT secSkill) {
  sstring desc;

  // What it is and who uses it
  desc += format("This %s is the kind of %s favored by %ss. ") % baseName %
              category % classInfo[classInd].name;

  // Craftsmanship
  desc += format("It appears to be %s but is otherwise unremarkable. ") %
              craftPhrases[static_cast<int>(quality)];

  // Magical properties
  desc += format("Something in the %s makes you think it might %s") %
              detailWord % statPhrases[stat];

  // Skill or secondary bonus (mutually exclusive by quality tier)
  sstring extra;
  if (bonusSkill != spellNumT(0))
    extra = skillPhrase(bonusSkill);
  else
    extra = secondaryBonusPhrase(sec, secSkill);

  if (!extra.empty())
    desc += format(" and %s") % extra;

  desc += ".\n\r";
  return desc;
}

// Attach an extra description to an object, keyed on its full keyword string
// so that any visible word in the item name triggers the description.
void addBulkExtraDesc(TObj* obj, const sstring& keywords,
    const sstring& description) {
  auto* ed = new extraDescription();
  ed->keyword = keywords;
  ed->description = description;
  ed->next = obj->ex_description;
  obj->ex_description = ed;
}


// -----------------------------------------------------------------------
// Set an apply on an object, using the next empty affected[] slot.
// Returns true on success, false if all slots are full.
// -----------------------------------------------------------------------
bool addObjApply(TObj* obj, applyTypeT apply, long mod, long mod2 = 0) {
  for (int i = 0; i < MAX_OBJ_AFFECT; ++i) {
    if (obj->affected[i].location == APPLY_NONE) {
      obj->affected[i].location = apply;
      obj->affected[i].modifier = mod;
      obj->affected[i].modifier2 = mod2;
      return true;
    }
  }
  return false;
}

bool addObjSkillBonus(TObj* obj, spellNumT skill, long amount) {
  return addObjApply(obj, APPLY_SPELL, skill, amount);
}

bool addObjImmunity(TObj* obj, immuneTypeT type, long amount) {
  return addObjApply(obj, APPLY_IMMUNITY, type, amount);
}

// -----------------------------------------------------------------------
// Weighted random stat selection
// Primary stats get 3x weight, secondaries get 1x
// Total weight: 3*3 + 9*1 = 18
// -----------------------------------------------------------------------
[[nodiscard]] statTypeT pickWeightedStat(classIndT classInd) {
  const auto& primaries = classPrimaryStats[classInd].stats;
  int roll = ::number(0, 17); // 0-17 = 18 outcomes

  // First 9 outcomes (3 primaries * 3x weight each)
  if (roll < 9)
    return primaries[roll / 3];

  // Remaining 9 outcomes = the 9 secondary stats
  int secondary = roll - 9;
  for (statTypeT stat = STAT_STR; stat < STAT_LUC; stat++) {
    bool isPrimary = false;
    for (auto p : primaries) {
      if (p == stat) {
        isPrimary = true;
        break;
      }
    }
    if (!isPrimary) {
      if (secondary == 0)
        return stat;
      --secondary;
    }
  }

  return STAT_STR; // unreachable
}

// -----------------------------------------------------------------------
// Weighted random skill selection
// Class skills get 3x weight; all other class skills are possible at 1x
// -----------------------------------------------------------------------
[[nodiscard]] spellNumT pickWeightedSkill(classIndT classInd) {
  const auto& pool = classSkillPools[classInd];
  if (pool.count == 0)
    return spellNumT(0);

  // Gather all unique skills from all classes
  std::vector<spellNumT> allSkills;
  for (const auto& cp : classSkillPools) {
    for (int i = 0; i < cp.count; ++i) {
      bool found = false;
      for (auto s : allSkills) {
        if (s == cp.skills[i]) {
          found = true;
          break;
        }
      }
      if (!found)
        allSkills.push_back(cp.skills[i]);
    }
  }

  // Build weighted pool: class skills at 3x, others at 1x
  int classWeight = pool.count * 3;
  int otherCount = static_cast<int>(allSkills.size()) - pool.count;
  int totalWeight = classWeight + otherCount;
  int roll = ::number(0, totalWeight - 1);

  if (roll < classWeight)
    return pool.skills[roll / 3];

  int secondary = roll - classWeight;
  for (auto skill : allSkills) {
    bool isClassSkill = false;
    for (int i = 0; i < pool.count; ++i) {
      if (pool.skills[i] == skill) {
        isClassSkill = true;
        break;
      }
    }
    if (!isClassSkill) {
      if (secondary == 0)
        return skill;
      --secondary;
    }
  }

  return pool.skills[0]; // unreachable
}

// -----------------------------------------------------------------------
// Broad skill selection for secondary bonus
// Pick a class (mob's class 3x weight, others 1x), then pick uniformly
// from that class's learnable skills. Only class-specific skills
// (not shared disciplines).
// -----------------------------------------------------------------------
[[nodiscard]] spellNumT pickBroadSkill(classIndT classInd) {
  // Pick a class: mob's class gets 3x weight
  // Total weight: 3 + 6*1 = 9 (7 real classes minus mob's = 6 others)
  constexpr int PLAYABLE_CLASSES = 7; // exclude Ranger, Commoner
  int totalWeight = 3 + (PLAYABLE_CLASSES - 1);
  int roll = ::number(0, totalWeight - 1);

  classIndT chosenClass;
  if (roll < 3) {
    chosenClass = classInd;
  } else {
    int idx = roll - 3;
    for (chosenClass = MIN_CLASS_IND; chosenClass < MAX_CLASSES; chosenClass++) {
      if (chosenClass == RANGER_LEVEL_IND || chosenClass == COMMONER_LEVEL_IND)
        continue;
      if (chosenClass == classInd)
        continue;
      if (idx == 0)
        break;
      --idx;
    }
  }

  int classNum = 1 << chosenClass;

  // Gather all learnable skills for the chosen class
  std::vector<spellNumT> pool;
  for (auto spell = MIN_SPELL; spell < MAX_SKILL; spell++) {
    if (!discArray[spell] || discArray[spell]->start <= 0)
      continue;
    auto disc = discArray[spell]->disc;
    if (disc == DISC_NONE || disc >= MAX_DISCS)
      continue;
    if (discNames[disc].class_num == classNum)
      pool.push_back(spell);
  }

  if (pool.empty())
    return spellNumT(0);

  return pool[::number(0, static_cast<int>(pool.size()) - 1)];
}

// -----------------------------------------------------------------------
// Apply the secondary bonus to an object
// -----------------------------------------------------------------------
// Apply the secondary bonus to an object.
// secSkill: pre-picked skill for SecondaryType::Skill (caller picks via
// pickBroadSkill so it can also be referenced in the item description).
void applySecondary(TObj* obj, SecondaryType sec, int bonus,
    spellNumT secSkill, double scale) {
  int scaledBonus = static_cast<int>(bonus * scale);
  switch (sec) {
    case SecondaryType::HP:
      addObjApply(obj, APPLY_HIT, scaledBonus);
      break;
    case SecondaryType::Mana:
      addObjApply(obj, APPLY_MANA, scaledBonus);
      break;
    case SecondaryType::Movement:
      addObjApply(obj, APPLY_MOVE, scaledBonus);
      break;
    case SecondaryType::Vision:
      addObjApply(obj, APPLY_VISION, scaledBonus);
      break;
    case SecondaryType::Skill:
      if (secSkill != spellNumT(0))
        addObjSkillBonus(obj, secSkill, scaledBonus);
      break;
    case SecondaryType::Glowing:
      obj->addObjStat(ITEM_GLOW);
      break;
    case SecondaryType::Shadowy:
      obj->addObjStat(ITEM_SHADOWY);
      break;
    case SecondaryType::ImmHeat:
      addObjImmunity(obj, IMMUNE_HEAT, scaledBonus);
      break;
    case SecondaryType::ImmCold:
      addObjImmunity(obj, IMMUNE_COLD, scaledBonus);
      break;
    case SecondaryType::ImmWater:
      addObjImmunity(obj, IMMUNE_WATER, scaledBonus);
      break;
    case SecondaryType::ImmEarth:
      addObjImmunity(obj, IMMUNE_EARTH, scaledBonus);
      break;
    case SecondaryType::ImmAir:
      addObjImmunity(obj, IMMUNE_AIR, scaledBonus);
      break;
    case SecondaryType::ImmLightning:
      addObjImmunity(obj, IMMUNE_ELECTRICITY, scaledBonus);
      break;
    case SecondaryType::CritFreq: {
      // CritFreq uses its own scale: raw bonus {3,5,7,9} → {1,2,3,4},
      // then multiplied by stat scale (1.5x for weapons/rings/shields).
      int critBonus = static_cast<int>((bonus - 1) / 2.0 * scale);
      addObjApply(obj, APPLY_CRIT_FREQUENCY, critBonus);
      break;
    }
    case SecondaryType::COUNT:
      break;
  }
}

// -----------------------------------------------------------------------
// Ring weight — fixed value since rings aren't race-sized
// -----------------------------------------------------------------------
inline constexpr double RING_WEIGHT = 0.1;

// -----------------------------------------------------------------------
// Generate a single bulk loot item for a given slot on a mob
// Returns the created object, or nullptr on failure
// -----------------------------------------------------------------------
[[nodiscard]] TObj* generateBulkItem(classIndT classInd, int level,
    race_t race, TemplateSlot templateSlot) {
  // Dead classes produce no bulk loot
  if (classInd == RANGER_LEVEL_IND || classInd == COMMONER_LEVEL_IND)
    return nullptr;

  const auto& tierInfo = armorTierByClass[classInd];
  auto quality = qualityFromLevel(level);
  const auto& qi = qualityTable[static_cast<int>(quality)];
  auto stat = pickWeightedStat(classInd);
  const auto& smi = statMaterials[stat];
  auto sec = pickSecondary(quality);
  const auto& secInfo = secondaryInfo[static_cast<int>(sec)];

  int mat = tierInfo.usesArmorMaterials ? smi.armorMat : smi.clothMat;
  const char* qualityName =
      tierInfo.usesArmorMaterials ? qi.armorName : qi.clothName;
  auto slotIdx = static_cast<int>(templateSlot);
  const char* baseName = classSlotNames[classInd][slotIdx];

  if (!baseName)
    return nullptr;

  bool isRing = (templateSlot == TemplateSlot::Finger);
  bool isShield = (templateSlot == TemplateSlot::Shield);

  // --- Determine racial size ---
  const auto* raceSize = raceSizeInfo(race);
  if (!raceSize)
    return nullptr;
  // Rings and shields don't display race size
  const char* sizeName = (isRing || isShield) ? nullptr : raceSize->name;

  // --- Compose short description ---
  // Format: "[<color>]a [pair of] [size] [quality] material baseName[<z>]"
  bool paired = (baseName[0] == 'p' && strcmp(baseName, "pants") == 0) ||
                (baseName[0] == 't' && strcmp(baseName, "trousers") == 0);
  // Determine the first descriptive word to choose a/an
  const char* firstWord = qualityName ? qualityName
                          : sizeName  ? sizeName
                                      : material_nums[mat].mat_name;
  bool useAn = strchr("aeiouAEIOU", firstWord[0]) != nullptr;

  sstring shortDesc;
  if (secInfo.ansiColor)
    shortDesc = secInfo.ansiColor;
  if (paired)
    shortDesc += "a pair of ";
  else
    shortDesc += useAn ? "an " : "a ";
  if (qualityName) {
    shortDesc += qualityName;
    shortDesc += " ";
  }
  if (sizeName) {
    shortDesc += sizeName;
    shortDesc += " ";
  }
  shortDesc += material_nums[mat].mat_name;
  shortDesc += " ";
  shortDesc += baseName;
  if (secInfo.ansiColor)
    shortDesc += "<z>";

  // --- Compose keywords ---
  sstring keywords = baseName;
  keywords += " ";
  keywords += material_nums[mat].mat_name;
  if (sizeName) {
    keywords += " ";
    keywords += sizeName;
  }
  if (qualityName) {
    keywords += " ";
    keywords += qualityName;
  }
  keywords += " [bulk]";
  if (!isRing && !isShield) {
    keywords += " [";
    keywords += raceSize->keyword;
    keywords += "]";
  }

  // --- Create the object ---
  itemTypeT itemType = isRing                        ? ITEM_JEWELRY
                       : tierInfo.usesArmorMaterials ? ITEM_ARMOR
                                                     : ITEM_WORN;
  TObj* obj = makeBlankWearable(itemType, templateSlot);
  if (!obj)
    return nullptr;

  // Bulk gear stays unrentable until the monogrammer personalizes it.
  obj->addObjStat(ITEM_NORENT);

  // --- Set basic properties ---
  obj->shortDescr = shortDesc;
  obj->name = keywords;
  obj->setDescr("A piece of equipment lies here.");

  const auto& slotInfo = templateSlots[slotIdx];
  obj->obj_flags.wear_flags = ITEM_WEAR_TAKE | slotInfo.wearFlag;

  obj->setMaterial(mat);
  obj->canBeSeen = BULK_CAN_BE_SEEN;
  obj->obj_flags.decay_time = OBJ_NOTIMER;

  // --- Anti-class flags ---
  if (tierInfo.antiFlags)
    obj->addObjStat(tierInfo.antiFlags);

  // --- Paired items (pants/trousers cover both legs) ---
  if (paired)
    obj->addObjStat(ITEM_PAIRED);

  double statScale = paired ? 2.0 : (isRing || isShield) ? 1.5 : 1.0;

  // --- Volume and weight ---
  if (isRing) {
    // Rings are not race-sized
    obj->setVolume(1);
    obj->setWeight(RING_WEIGHT);
  } else if (isShield) {
    // Shields are not race-sized
    int volume = slotBaseVolumes[slotIdx];
    obj->setVolume(volume);
    obj->setWeight(volume * material_density[mat] * WEIGHT_CONSTANT);
  } else {
    int volume =
        static_cast<int>(slotBaseVolumes[slotIdx] * raceSize->modifier);
    if (paired)
      volume *= 2;
    obj->setVolume(volume);
    obj->setWeight(volume * material_density[mat] * WEIGHT_CONSTANT);
  }

  // --- AC and struct points via setDefArmorLevel ---
  if (auto* clothing = dynamic_cast<TBaseClothing*>(obj)) {
    // setDefArmorLevel needs an APPLY_ARMOR slot to exist
    addObjApply(obj, APPLY_ARMOR, 0);

    int cappedLevel = std::min(level, 60);
    double acLevel = cappedLevel * tierInfo.acScale;
    if (isRing)
      acLevel *= 0.5;
    clothing->setDefArmorLevel(static_cast<float>(acLevel));

    // Adjust struct points for quality and rings
    double structPct = qi.structPct;
    if (isRing)
      structPct *= 0.5;
    if (structPct < 1.0) {
      int maxStruct = clothing->getMaxStructPoints();
      clothing->setStructPoints(
          std::max(1, static_cast<int>(maxStruct * structPct)));
    }
  }

  // --- Stat bonus ---
  addObjApply(obj, smi.apply, static_cast<long>(qi.statBonus * statScale));

  // --- Skill bonus (low tiers) ---
  spellNumT bonusSkill = spellNumT(0);
  if (qi.skillBonus > 0) {
    bonusSkill = pickWeightedSkill(classInd);
    if (bonusSkill != spellNumT(0))
      addObjSkillBonus(obj, bonusSkill,
          static_cast<long>(qi.skillBonus * statScale));
  }

  // --- Secondary bonus ---
  spellNumT secSkill = spellNumT(0);
  if (qi.secondaryBonus > 0) {
    if (sec == SecondaryType::Skill)
      secSkill = pickBroadSkill(classInd);
    applySecondary(obj, sec, qi.secondaryBonus, secSkill, statScale);
  }

  // --- Extra description ---
  const char* category = tierInfo.usesArmorMaterials ? "armor" : "clothing";
  const char* detailWord = tierInfo.usesArmorMaterials ? "engraving" : "stitching";
  addBulkExtraDesc(obj, keywords,
      composeBulkDescription(baseName, category, detailWord, classInd, quality,
          stat, bonusSkill, sec, secSkill));

  // --- Price (must be set after all other properties) ---
  if (auto* clothing = dynamic_cast<TBaseClothing*>(obj))
    obj->obj_flags.cost = clothing->suggestedPrice();

  return obj;
}

// -----------------------------------------------------------------------
// Weapon system — physical specs, class pools, generation
// -----------------------------------------------------------------------

// Every weapon that appears in any class pool, with fixed physical properties.
// Organized by proficiency category and handedness for readability.
struct WeaponSpec {
  const char* name;
  int volume;
  int maxSharp;
  bool twoHanded;
};

enum class WeaponId : int {
  // One-handed pierce
  Dagger, Dirk, Epee, Gladius, Harpoon, Katar, Knife, Kris, Misericorde,
  Pick, Pickaxe, Poniard, Rapier, Sai, Shortsword, Spear, Stiletto, Tanto,
  // Two-handed pierce
  Estoc, Falx, Lance, Pike, Ranseur, Trident, Warpick,
  // One-handed slash
  Axe, Battleaxe, Broadsword, Cleaver, Cutlass, Falchion, Hatchet, Katana,
  Khopesh, Kukri, Longsword, Machete, Saber, Scimitar, Sickle, Sword, Tachi,
  Tomahawk, Wakizashi,
  // Two-handed slash
  Claymore, Fauchard, Flamberge, Greataxe, Halberd, Naginata, Scythe,
  Shamshir, Tulwar, Waraxe, Warblade, Zanbatou,
  // One-handed blunt
  Baton, Cane, Club, Cudgel, Flail, Hammer, Mace, Mallet, Morningstar,
  Scepter, Truncheon, Warhammer,
  // Two-handed blunt
  Greathammer, Kanabo, Martel, Mattock, Maul, Quarterstaff, Sledgehammer,
  SpadeWeap, Staff, WarClub,
  COUNT
};

inline constexpr int WEAPON_ID_COUNT = static_cast<int>(WeaponId::COUNT);

// clang-format off
inline constexpr std::array<WeaponSpec, WEAPON_ID_COUNT> weaponSpecs = {{
  // 1h pierce          name              vol  sharp  2h
  /* Dagger       */ {"dagger",            450,  70, false},
  /* Dirk         */ {"dirk",              500,  70, false},
  /* Epee         */ {"epee",             3000,  85, false},
  /* Gladius      */ {"gladius",          1000,  65, false},
  /* Harpoon      */ {"harpoon",          4000,  50, false},
  /* Katar        */ {"katar",             800,  85, false},
  /* Knife        */ {"knife",             600,  65, false},
  /* Kris         */ {"kris",              700,  80, false},
  /* Misericorde  */ {"misericorde",       450,  90, false},
  /* Pick         */ {"pick",             1200,  55, false},
  /* Pickaxe      */ {"pickaxe",          1200,  50, false},
  /* Poniard      */ {"poniard",          1200,  75, false},
  /* Rapier       */ {"rapier",           1500,  85, false},
  /* Sai          */ {"sai",               550,  55, false},
  /* Shortsword   */ {"shortsword",       1000,  65, false},
  /* Spear        */ {"spear",            2500,  55, false},
  /* Stiletto     */ {"stiletto",          650,  95, false},
  /* Tanto        */ {"tanto",            1600,  85, false},
  // 2h pierce
  /* Estoc        */ {"estoc",            8000,  85, true},
  /* Falx         */ {"falx",             7000,  70, true},
  /* Lance        */ {"lance",            4000,  50, true},
  /* Pike         */ {"pike",             2000,  50, true},
  /* Ranseur      */ {"ranseur",          7000,  60, true},
  /* Trident      */ {"trident",          3000,  60, true},
  /* Warpick      */ {"warpick",          3000,  65, true},
  // 1h slash
  /* Axe          */ {"axe",              3000,  60, false},
  /* Battleaxe    */ {"battleaxe",        3000,  65, false},
  /* Broadsword   */ {"broadsword",       8000,  65, false},
  /* Cleaver      */ {"cleaver",          7000,  60, false},
  /* Cutlass      */ {"cutlass",          2200,  70, false},
  /* Falchion     */ {"falchion",         7000,  70, false},
  /* Hatchet      */ {"hatchet",          1500,  55, false},
  /* Katana       */ {"katana",           8000,  95, false},
  /* Khopesh      */ {"khopesh",          3500,  65, false},
  /* Kukri        */ {"kukri",             800,  75, false},
  /* Longsword    */ {"longsword",        7000,  70, false},
  /* Machete      */ {"machete",           800,  60, false},
  /* Saber        */ {"saber",            2500,  75, false},
  /* Scimitar     */ {"scimitar",         7000,  75, false},
  /* Sickle       */ {"sickle",           1500,  65, false},
  /* Sword        */ {"sword",            6000,  65, false},
  /* Tachi        */ {"tachi",            1500,  90, false},
  /* Tomahawk     */ {"tomahawk",         1500,  55, false},
  /* Wakizashi    */ {"wakizashi",         800,  90, false},
  // 2h slash
  /* Claymore     */ {"claymore",         8000,  60, true},
  /* Fauchard     */ {"fauchard",         7000,  65, true},
  /* Flamberge    */ {"flamberge",        8000,  70, true},
  /* Greataxe     */ {"greataxe",         5000,  60, true},
  /* Halberd      */ {"halberd",          3400,  60, true},
  /* Naginata     */ {"naginata",         2500,  80, true},
  /* Scythe       */ {"scythe",           3000,  70, true},
  /* Shamshir     */ {"shamshir",         8000,  80, true},
  /* Tulwar       */ {"tulwar",           8000,  75, true},
  /* Waraxe       */ {"waraxe",           5000,  60, true},
  /* Warblade     */ {"warblade",         8000,  70, true},
  /* Zanbatou     */ {"zanbatou",         9000,  60, true},
  // 1h blunt
  /* Baton        */ {"baton",             330,  25, false},
  /* Cane         */ {"cane",             2500,  25, false},
  /* Club         */ {"club",             3000,  35, false},
  /* Cudgel       */ {"cudgel",           1000,  35, false},
  /* Flail        */ {"flail",            2400,  70, false},
  /* Hammer       */ {"hammer",           2000,  55, false},
  /* Mace         */ {"mace",             3000,  65, false},
  /* Mallet       */ {"mallet",           3000,  40, false},
  /* Morningstar  */ {"morningstar",      2400,  75, false},
  /* Scepter      */ {"scepter",          2500,  45, false},
  /* Truncheon    */ {"truncheon",         330,  30, false},
  /* Warhammer    */ {"warhammer",        3200,  70, false},
  // 2h blunt
  /* Greathammer  */ {"greathammer",      5000,  60, true},
  /* Kanabo       */ {"kanabo",           4000,  65, true},
  /* Martel       */ {"martel",           4000,  70, true},
  /* Mattock      */ {"mattock",          3000,  60, true},
  /* Maul         */ {"maul",             3000,  50, true},
  /* Quarterstaff */ {"quarterstaff",     4000,  25, true},
  /* Sledgehammer */ {"sledgehammer",     5000,  45, true},
  /* SpadeWeap    */ {"spade",            4000,  50, true},
  /* Staff        */ {"staff",            4000,  20, true},
  /* WarClub      */ {"war club",         3500,  45, true},
}};
// clang-format on

// A weapon + its damage type configuration, used in class pools.
struct WeaponPoolEntry {
  WeaponId id;
  weaponT type1;
  int freq1;       // 70 or 100
  weaponT type2;   // WEAPON_TYPE_NONE if pure
  int freq2;       // 30 or 0
};

// Shorthand for defining pool entries
#define W1(wid, t1) \
  WeaponPoolEntry{WeaponId::wid, WEAPON_TYPE_##t1, 100, WEAPON_TYPE_NONE, 0}
#define W2(wid, t1, t2) \
  WeaponPoolEntry{WeaponId::wid, WEAPON_TYPE_##t1, 70, WEAPON_TYPE_##t2, 30}

// clang-format off
inline constexpr std::array universalWeapons = {
  W1(Cane,       STRIKE),
  W1(Club,       SMASH),
  W1(Hammer,     BLUDGEON),
  W2(Knife,      STAB, SLICE),
  W1(Spear,      SPEAR),
  W1(Staff,      STRIKE),
  W2(Sword,      SLASH, THRUST),
};

inline constexpr std::array warriorWeapons = {
  W1(Baton,      STRIKE),
  W2(Battleaxe,  CLEAVE, STRIKE),
  W2(Broadsword, SLASH, THRUST),
  W2(Claymore,   CLEAVE, THRUST),
  W1(Cleaver,    CLEAVE),
  W2(Cutlass,    SLASH, STAB),
  W2(Falchion,   CLEAVE, SLICE),
  W2(Flail,      BLUDGEON, STRIKE),
  W2(Gladius,    THRUST, SLASH),
  W1(Greataxe,   CLEAVE),
  W2(Halberd,    CLEAVE, THRUST),
  W1(Harpoon,    SPEAR),
  W2(Hatchet,    CLEAVE, SLASH),
  W2(Katana,     SLICE, THRUST),
  W1(Lance,      THRUST),
  W2(Longsword,  SLASH, THRUST),
  W1(Mace,       BLUDGEON),
  W2(Mattock,    BLUDGEON, STAB),
  W1(Maul,       SMASH),
  W2(Morningstar,SMASH, STAB),
  W2(Naginata,   SLICE, SPEAR),
  W2(Pickaxe,    STAB, STRIKE),
  W1(Pike,       SPEAR),
  W2(Shamshir,   SLICE, SLASH),
  W2(Tomahawk,   SLASH, STRIKE),
  W2(Trident,    SPEAR, SLASH),
  W2(Tulwar,     SLASH, SLICE),
  W2(WarClub,    SMASH, SLASH),
  W2(Waraxe,     CLEAVE, BLUDGEON),
  W2(Warblade,   SLASH, SMASH),
  W2(Warhammer,  BLUDGEON, THRUST),
  W2(Warpick,    THRUST, BLUDGEON),
};

inline constexpr std::array deikhanWeapons = {
  W2(Claymore,   CLEAVE, THRUST),
  W1(Estoc,      THRUST),
  W2(Falx,       THRUST, SLICE),
  W2(Flamberge,  SLASH, SLICE),
  W2(Khopesh,    SLASH, STAB),
  W1(Lance,      THRUST),
  W2(Longsword,  SLASH, THRUST),
  W1(Mace,       BLUDGEON),
  W2(Martel,     BLUDGEON, STRIKE),
  W2(Mattock,    BLUDGEON, STAB),
  W1(Maul,       SMASH),
  W1(Pike,       SPEAR),
  W2(Ranseur,    SPEAR, THRUST),
  W2(Saber,      SLASH, SLICE),
  W2(Scythe,     SLICE, SLASH),
  W1(Scepter,    BLUDGEON),
  W2(Tulwar,     SLASH, SLICE),
  W2(Zanbatou,   CLEAVE, SLASH),
};

inline constexpr std::array clericWeapons = {
  W1(Baton,        STRIKE),
  W1(Cudgel,       BLUDGEON),
  W2(Flail,        BLUDGEON, STRIKE),
  W1(Greathammer,  SMASH),
  W1(Hammer,       BLUDGEON),
  W1(Mace,         BLUDGEON),
  W1(Mallet,       SMASH),
  W2(Martel,       BLUDGEON, STRIKE),
  W1(Maul,         SMASH),
  W2(Morningstar,  SMASH, STAB),
  W2(Quarterstaff, STRIKE, BLUDGEON),
  W1(Scepter,      BLUDGEON),
  W2(Warhammer,    BLUDGEON, THRUST),
};

inline constexpr std::array thiefWeapons = {
  W1(Baton,      STRIKE),
  W1(Cudgel,     BLUDGEON),
  W2(Cutlass,    SLASH, STAB),
  W1(Dagger,     STAB),
  W2(Dirk,       STAB, THRUST),
  W1(Epee,       THRUST),
  W2(Fauchard,   SLASH, SPEAR),
  W1(Harpoon,    SPEAR),
  W2(Katana,     SLICE, THRUST),
  W1(Katar,      THRUST),
  W2(Khopesh,    SLASH, STAB),
  W2(Kris,       STAB, SLICE),
  W2(Kukri,      SLASH, CLEAVE),
  W2(Naginata,   SLICE, SPEAR),
  W1(Poniard,    STAB),
  W2(Ranseur,    SPEAR, THRUST),
  W1(Rapier,     THRUST),
  W2(Sai,        STAB, STRIKE),
  W1(Scimitar,   SLASH),
  W2(Shortsword, THRUST, SLASH),
  W1(Stiletto,   STAB),
  W2(Tachi,      SLASH, SLICE),
  W2(Tanto,      STAB, SLASH),
  W2(Wakizashi,  SLASH, STAB),
};

inline constexpr std::array mageWeapons = {
  W1(Dagger,      STAB),
  W2(Kris,        STAB, SLICE),
  W1(Mace,        BLUDGEON),
  W1(Misericorde, THRUST),
  W1(Scimitar,    SLASH),
  W1(Scepter,     BLUDGEON),
  W2(Sickle,      SLASH, STAB),
};

inline constexpr std::array shamanWeapons = {
  W1(Axe,        CLEAVE),
  W1(Cleaver,    CLEAVE),
  W1(Harpoon,    SPEAR),
  W2(Hatchet,    CLEAVE, SLASH),
  W1(Katar,      THRUST),
  W2(Kukri,      SLASH, CLEAVE),
  W2(Machete,    SLASH, CLEAVE),
  W1(Mallet,     SMASH),
  W2(Mattock,    BLUDGEON, STAB),
  W1(Maul,       SMASH),
  W2(Pick,       STAB, BLUDGEON),
  W2(Pickaxe,    STAB, STRIKE),
  W2(Scythe,     SLICE, SLASH),
  W2(Sickle,     SLASH, STAB),
  W2(SpadeWeap,  STRIKE, SLASH),
  W2(Tomahawk,   SLASH, STRIKE),
  W2(Trident,    SPEAR, SLASH),
  W1(Truncheon,  STRIKE),
  W2(WarClub,    SMASH, SLASH),
  W2(Zanbatou,   CLEAVE, SLASH),
};

inline constexpr std::array monkWeapons = {
  W1(Baton,        STRIKE),
  W1(Epee,         THRUST),
  W2(Kanabo,       SMASH, STAB),
  W1(Mace,         BLUDGEON),
  W1(Maul,         SMASH),
  W1(Pike,         SPEAR),
  W2(Quarterstaff, STRIKE, BLUDGEON),
  W2(Ranseur,      SPEAR, THRUST),
  W1(Rapier,       THRUST),
  W2(Sai,          STAB, STRIKE),
  W2(Shortsword,   THRUST, SLASH),
  W1(Sledgehammer, SMASH),
  W2(SpadeWeap,    STRIKE, SLASH),
  W2(Tanto,        STAB, SLASH),
};
// clang-format on

#undef W1
#undef W2

// Returns (data pointer, count) for a class's weapon pool.
// Universal weapons are separate and always included.
struct WeaponPool {
  const WeaponPoolEntry* entries;
  int count;
};

[[nodiscard]] WeaponPool classWeaponPool(classIndT classInd) {
  switch (classInd) {
    // clang-format off
    case WARRIOR_LEVEL_IND: return {warriorWeapons.data(), static_cast<int>(warriorWeapons.size())};
    case DEIKHAN_LEVEL_IND: return {deikhanWeapons.data(), static_cast<int>(deikhanWeapons.size())};
    case CLERIC_LEVEL_IND:  return {clericWeapons.data(),  static_cast<int>(clericWeapons.size())};
    case THIEF_LEVEL_IND:   return {thiefWeapons.data(),   static_cast<int>(thiefWeapons.size())};
    case MAGE_LEVEL_IND:    return {mageWeapons.data(),    static_cast<int>(mageWeapons.size())};
    case SHAMAN_LEVEL_IND:  return {shamanWeapons.data(),  static_cast<int>(shamanWeapons.size())};
    case MONK_LEVEL_IND:    return {monkWeapons.data(),    static_cast<int>(monkWeapons.size())};
    default:                return {nullptr, 0};
    // clang-format on
  }
}

// -----------------------------------------------------------------------
// Generate a bulk loot weapon for a mob
// Returns the created weapon, or nullptr on failure
// -----------------------------------------------------------------------
[[nodiscard]] TObj* generateBulkWeapon(classIndT classInd, int level) {
  if (classInd == RANGER_LEVEL_IND || classInd == COMMONER_LEVEL_IND)
    return nullptr;

  auto pool = classWeaponPool(classInd);
  if (!pool.entries)
    return nullptr;

  // Pick from combined pool: class weapons + universal weapons
  int totalCount =
      pool.count + static_cast<int>(universalWeapons.size());
  int roll = ::number(0, totalCount - 1);

  const WeaponPoolEntry& entry =
      roll < pool.count ? pool.entries[roll]
                        : universalWeapons[roll - pool.count];
  const auto& spec = weaponSpecs[static_cast<int>(entry.id)];

  // Stat, quality, material (weapons always use metal/armor materials)
  auto stat = pickWeightedStat(classInd);
  auto quality = qualityFromLevel(level);
  const auto& smi = statMaterials[stat];
  const auto& qi = qualityTable[static_cast<int>(quality)];
  auto sec = pickSecondary(quality);
  const auto& secInfo = secondaryInfo[static_cast<int>(sec)];
  int mat = smi.armorMat;
  const char* qualityName = qi.armorName;

  // --- Compose short description ---
  // Format: "[<color>]a [quality] material weaponName[<z>]"
  const char* firstWord =
      qualityName ? qualityName : material_nums[mat].mat_name;
  bool useAn = strchr("aeiouAEIOU", firstWord[0]) != nullptr;

  sstring shortDesc;
  if (secInfo.ansiColor)
    shortDesc = secInfo.ansiColor;
  shortDesc += useAn ? "an " : "a ";
  if (qualityName) {
    shortDesc += qualityName;
    shortDesc += " ";
  }
  shortDesc += material_nums[mat].mat_name;
  shortDesc += " ";
  shortDesc += spec.name;
  if (secInfo.ansiColor)
    shortDesc += "<z>";

  // --- Compose keywords ---
  sstring keywords = spec.name;
  keywords += " ";
  keywords += material_nums[mat].mat_name;
  if (qualityName) {
    keywords += " ";
    keywords += qualityName;
  }
  keywords += " [bulk]";

  // --- Create the weapon ---
  auto* weapon = new TGenWeapon();
  weapon->addObjStat(ITEM_STRUNG);
  weapon->addObjStat(ITEM_NORENT);

  weapon->shortDescr = shortDesc;
  weapon->name = keywords;
  weapon->setDescr("A weapon lies here.");

  weapon->obj_flags.wear_flags = ITEM_WEAR_TAKE | ITEM_WEAR_HOLD;
  if (spec.twoHanded)
    weapon->addObjStat(ITEM_PAIRED);

  weapon->setMaterial(mat);
  weapon->canBeSeen = BULK_CAN_BE_SEEN;
  weapon->obj_flags.decay_time = OBJ_NOTIMER;

  // --- Volume and weight ---
  weapon->setVolume(spec.volume);
  weapon->setWeight(spec.volume * material_density[mat] * WEIGHT_CONSTANT);

  // --- Weapon combat stats via editAverageMe formula ---
  // Same as oedit "average" command: given a level, derive struct, damage,
  // sharpness, and deviation. We use the weapon spec's sharpness instead of
  // the formula's.
  double scaledLevel = std::min(static_cast<double>(level), 60.0);
  int maxStruct = std::max(
      1, static_cast<int>(scaledLevel * 1.5 + 10.0));
  int damLevel = std::clamp(static_cast<int>(scaledLevel * 4.0), 0, 255);
  int damDev =
      std::max(0, static_cast<int>(10.0 - (scaledLevel / 60.0) * 10.0));

  // Scale struct by quality (flimsy weapons break faster)
  maxStruct = std::max(1, static_cast<int>(maxStruct * qi.structPct));

  weapon->setMaxSharp(spec.maxSharp);
  weapon->setCurSharp(spec.maxSharp);
  weapon->setWeapDamLvl(damLevel);
  weapon->setWeapDamDev(damDev);
  weapon->setMaxStructPoints(static_cast<short>(maxStruct));
  weapon->setStructPoints(static_cast<short>(maxStruct));

  // --- Damage types ---
  weapon->setWeaponType(entry.type1, 0);
  weapon->setWeaponFreq(entry.freq1, 0);
  if (entry.type2 != WEAPON_TYPE_NONE) {
    weapon->setWeaponType(entry.type2, 1);
    weapon->setWeaponFreq(entry.freq2, 1);
  }

  // --- Stat bonus (1.5x for weapons, doubled again if two-handed
  // since it occupies both hand slots) ---
  double statScale = spec.twoHanded ? 3.0 : 1.5;
  addObjApply(weapon, smi.apply,
      static_cast<long>(qi.statBonus * statScale));

  // --- Skill bonus (low tiers) ---
  spellNumT bonusSkill = spellNumT(0);
  if (qi.skillBonus > 0) {
    bonusSkill = pickWeightedSkill(classInd);
    if (bonusSkill != spellNumT(0))
      addObjSkillBonus(weapon, bonusSkill,
          static_cast<long>(qi.skillBonus * statScale));
  }

  // --- Secondary bonus ---
  spellNumT secSkill = spellNumT(0);
  if (qi.secondaryBonus > 0) {
    if (sec == SecondaryType::Skill)
      secSkill = pickBroadSkill(classInd);
    applySecondary(weapon, sec, qi.secondaryBonus, secSkill, statScale);
  }

  // --- Extra description ---
  addBulkExtraDesc(weapon, keywords,
      composeBulkDescription(spec.name, "weapon", "balance", classInd,
          quality, stat, bonusSkill, sec, secSkill));

  // --- Price (must be set after all other properties) ---
  weapon->obj_flags.cost = weapon->suggestedPrice();

  return weapon;
}

// -----------------------------------------------------------------------
// Commodity purchase — deduct raw materials from commodity shops
// Mirrors the commodity branch of read_object_buy_build() in db.cc.
// Given a generated item's material and weight, finds a matching commodity
// in a shop and buys from it, reducing the commodity's weight.
// -----------------------------------------------------------------------
void buyCommodityForItem(TMonster* buyer, int material, float weight) {
  if (weight <= 0)
    return;

  TDatabase db(DB_SNEEZY);
  db.query(
      "select r.owner as shop_nr, r.rent_id as rent_id, r.material as material,"
      " r.weight*10 as units from rent r, obj o where o.type=%i and"
      " r.vnum=o.vnum and r.material=%i and owner_type='shop' and"
      " r.weight>=%f order by units desc",
      ITEM_RAW_MATERIAL, material, weight);

  if (!db.fetchRow())
    return;

  int shop_nr = convertTo<int>(db["shop_nr"]);
  int rent_id = convertTo<int>(db["rent_id"]);
  TShopOwned tso(shop_nr, buyer);

  if (!tso.getKeeper())
    return;

  TObj* obj = tso.getKeeper()->loadItem(shop_nr, rent_id);
  if (!obj)
    return;

  auto* commod = dynamic_cast<TCommodity*>(obj);
  if (!commod) {
    delete obj;
    return;
  }

  *tso.getKeeper() += *commod;
  int price = commod->shopPrice(
      static_cast<int>(weight * 10), shop_nr, -1, buyer);

  buyer->addToMoney(price, GOLD_XFER);
  tso.doBuyTransaction(price, commod->getName(), TX_BUYING, commod);

  commod->setWeight(commod->getWeight() - weight);
  vlogf(LOG_PEEL,
      format("%s purchased %s (%i) from shop %i for %i talens. [bulk]") %
          buyer->getName() % commod->getName() % static_cast<int>(weight * 10) %
          shop_nr % price);

  tso.getKeeper()->deleteItem(shop_nr, rent_id);
  if (commod->getWeight() > 0)
    tso.getKeeper()->saveItem(shop_nr, commod);

  --(*commod);
  delete commod;
}

}  // anonymous namespace

int slotVolumeForRace(TemplateSlot slot, race_t race) {
  if (slot == TemplateSlot::COUNT)
    return 0;

  const RaceSizeInfo* size = raceSizeInfo(race);
  if (!size)
    return 0;

  return static_cast<int>(
    slotBaseVolumes[static_cast<int>(slot)] * size->modifier);
}

bool findWeaponSpecByName(const char* name, ForgeWeaponSpec* out) {
  if (!name || !*name || !out)
    return false;

  // The damage types live in the class pools rather than in weaponSpecs, so
  // the lookup has to walk both: the pools say what a weapon does, the spec
  // table says what it is.
  for (int i = 0; i < WEAPON_ID_COUNT; i++) {
    if (strcasecmp(weaponSpecs[i].name, name))
      continue;

    // Each pool is a std::array of its own size, so they are different types
    // and cannot be walked through one list of pointers. A generic lambda can
    // take them one at a time.
    const WeaponPoolEntry* entry = nullptr;
    auto scan = [&](const auto& pool) {
      if (entry)
        return;
      for (const auto& candidate : pool) {
        if (static_cast<int>(candidate.id) == i) {
          entry = &candidate;
          return;
        }
      }
    };

    scan(universalWeapons);
    scan(warriorWeapons);
    scan(deikhanWeapons);
    scan(clericWeapons);
    scan(thiefWeapons);
    scan(mageWeapons);
    scan(shamanWeapons);
    scan(monkWeapons);

    if (!entry)
      return false;

    out->name = weaponSpecs[i].name;
    out->volume = weaponSpecs[i].volume;
    out->maxSharp = weaponSpecs[i].maxSharp;
    out->twoHanded = weaponSpecs[i].twoHanded;
    out->type1 = static_cast<int>(entry->type1);
    out->freq1 = entry->freq1;
    out->type2 = static_cast<int>(entry->type2);
    out->freq2 = entry->freq2;

    return true;
  }

  return false;
}

const char* raceSizeName(race_t race) {
  const RaceSizeInfo* size = raceSizeInfo(race);
  return size ? size->name : nullptr;
}

float weightForVolume(int volume, int material) {
  if (volume <= 0 || material < 0 || material >= 200)
    return 0.0f;

  return static_cast<float>(volume * material_density[material] *
                            WEIGHT_CONSTANT);
}

int volumeForWeight(float weight, int material) {
  if (weight <= 0.0f || material < 0 || material >= 200)
    return 0;

  // Density is zero for materials the table never filled in; there is no
  // volume that answers for those, so report none rather than divide by it.
  double density = material_density[material];
  if (density <= 0.0)
    return 0;

  return static_cast<int>(weight / (density * WEIGHT_CONSTANT));
}

// -----------------------------------------------------------------------
// Public interface: attempt to generate bulk loot on a humanoid mob
// Called during zone reset for each mob
// -----------------------------------------------------------------------
void bulkLoadOut(TMonster* mob) {
  if (!mob || !mob->isHumanoid())
    return;

  if (!raceSizeInfo(mob->getRace()))
    return;

  auto classInd = mob->bestClass();
  if (classInd == RANGER_LEVEL_IND || classInd == COMMONER_LEVEL_IND)
    return;

  for (int i = 0; i < TEMPLATE_SLOT_COUNT; ++i) {
    auto templateSlot = static_cast<TemplateSlot>(i);
    // A shield belongs in the off hand, leaving the weapon hand free below.
    // slot_from_bit answers ITEM_WEAR_HOLD with a fixed side, so ask the mob
    // which side that is rather than hardcoding one.
    wearSlotT wearSlot = (templateSlot == TemplateSlot::Shield)
                           ? mob->getSecondaryHold()
                           : slot_from_bit(templateSlots[i].wearFlag);

    // Don't generate if mob already has something in this slot
    if (mob->equipment[wearSlot])
      continue;

    if (!percentChance(BULK_LOAD_CHANCE_PCT))
      continue;

    TObj* item = generateBulkItem(classInd, mob->GetMaxLevel(),
        mob->getRace(), templateSlot);
    if (item) {
      buyCommodityForItem(mob, item->getMaterial(),
          static_cast<float>(item->getWeight()));
      mob->equipChar(item, wearSlot, SILENT_YES);
    }
  }

  // --- Weapon roll (independent of armor slots) ---
  // Mobs are left-handed: PLR_RT_HANDED is only ever set during character
  // creation, so isRightHanded() is false for every one of them.  Go through
  // the handedness helpers anyway - they are what decides which hold is the
  // weapon hand, and hardcoding a side here would fight the shield above.
  wearSlotT weaponHold = mob->getPrimaryHold();
  wearSlotT offHold = mob->getSecondaryHold();

  if (mob->equipment[weaponHold])
    return;
  // Two-handed weapons need both hands free
  bool offHandFree = !mob->equipment[offHold];

  if (!percentChance(BULK_LOAD_CHANCE_PCT))
    return;

  if (auto* weapon = generateBulkWeapon(classInd, mob->GetMaxLevel())) {
    if (weapon->isPaired() && !offHandFree) {
      delete weapon;
      return;
    }
    buyCommodityForItem(mob, weapon->getMaterial(),
        static_cast<float>(weapon->getWeight()));
    mob->equipChar(weapon, weaponHold, SILENT_YES);
  }
}

// -----------------------------------------------------------------------
// Public interface: generate one loose bulk loot item
// -----------------------------------------------------------------------
[[nodiscard]] TObj* bulkLoadOutItem(classIndT classInd, int level,
    race_t race) {
  // The weapon competes as one more slot alongside the armor slots
  int roll = ::number(0, TEMPLATE_SLOT_COUNT);
  if (roll == TEMPLATE_SLOT_COUNT)
    return generateBulkWeapon(classInd, level);

  return generateBulkItem(classInd, level, race, static_cast<TemplateSlot>(roll));
}
