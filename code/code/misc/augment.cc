//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
// augment.cc - gear augmentation skill entry points
//
//   The eleven skills of the gear augmentation system, registered and
//   reachable but not yet implemented.  Each will grow its own task file as
//   its behavior is decided; see the design at
//   docs/superpowers/specs/2026-08-23-gear-augmentation-design.md, whose Open
//   Items note that success rates, costs and failure behavior are settled for
//   none of them yet.
//
//   Until then every entry point does the one thing that is already decided:
//   gate on knowing the skill, so the command surface and the skill wiring can
//   be exercised ahead of the mechanics.
//
//////////////////////////////////////////////////////////////////////////

#include "comm.h"
#include "extern.h"
#include "being.h"
#include "handler.h"
#include "augment.h"
#include "obj_base_clothing.h"
#include "obj_low.h"
#include "task.h"
#include "materials.h"
#include "bulkLoadOut.h"
#include "obj_commodity.h"
#include "obj_ingot.h"
#include "obj_opal.h"
#include "obj_soulstone.h"
#include "obj_base_corpse.h"
#include "obj_vial.h"
#include "obj_symbol.h"
#include "obj_essence.h"
#include "obj_skein.h"
#include "obj_general_weapon.h"
#include "wearTemplate.h"

unsigned int getTierRungFlags(Tier tier) {
  // The masks ArmorEvaluator::getTier() layers: clothing carries nothing,
  // light adds mage and shaman, medium adds monk and thief, heavy adds cleric
  // and ranger. A rung owns the pair its own layer added, so clearing that
  // pair is exactly one step down.
  switch (tier) {
    case Tier_Heavy:
      return ITEM_ANTI_CLERIC | ITEM_ANTI_RANGER;
    case Tier_Medium:
      return ITEM_ANTI_MONK | ITEM_ANTI_THIEF;
    case Tier_Light:
      return ITEM_ANTI_MAGE | ITEM_ANTI_SHAMAN;
    default:
      return 0;
  }
}

Tier getWearableTier(const TBaseClothing* clothing) {
  if (!clothing)
    return Tier_Max;

  ArmorEvaluator eval(clothing);
  return eval.getTier();
}

TemplateSlot getWearableSlot(const TObj* obj) {
  if (!obj)
    return TemplateSlot::COUNT;

  // ITEM_WEAR_TAKE and ITEM_WEAR_THROW are not slots, and an item carrying two
  // real slot bits has no single template to become -- the spec checked the
  // world data and found none, so this is a guard, not a case to handle.
  TemplateSlot found = TemplateSlot::COUNT;
  for (const auto& info : templateSlots) {
    if (!(obj->obj_flags.wear_flags & info.wearFlag))
      continue;
    if (found != TemplateSlot::COUNT)
      return TemplateSlot::COUNT;
    found = info.slot;
  }

  return found;
}

TObj* convertWearableType(TBeing* ch, TObj* obj, itemTypeT type) {
  TemplateSlot slot = getWearableSlot(obj);
  if (slot == TemplateSlot::COUNT)
    return nullptr;

  TObj* fresh = makeBlankWearable(type, slot);
  if (!fresh)
    return nullptr;

  // The template owns the wear flags -- they are what rent rebuilds the item
  // from -- so hold them aside across the flag-word copy. They match the
  // original's slot by construction; augmentation never moves an item's slot.
  unsigned int wearFlags = fresh->obj_flags.wear_flags;
  fresh->obj_flags = obj->obj_flags;
  fresh->obj_flags.wear_flags = wearFlags;

  // makeBlankWearable() strung the copy so it owns its strings. The flag word
  // just overwrote that bit with the original's, which may not have been
  // strung, so put it back before writing any of the strings below.
  fresh->addObjStat(ITEM_STRUNG);

  for (int i = 0; i < MAX_OBJ_AFFECT; i++)
    fresh->affected[i] = obj->affected[i];

  fresh->name = obj->name;
  fresh->shortDescr = obj->shortDescr;
  fresh->setDescr(obj->getDescr());
  fresh->action_description = obj->action_description;
  fresh->setMaterial(obj->getMaterial());
  fresh->setWeight(obj->getWeight());
  fresh->canBeSeen = obj->canBeSeen;

  // Strip only works on carried items, so the splice is inventory to
  // inventory.
  // Unhook the original before deleting it, and leave it with no parent.
  --(*obj);
  *ch += *fresh;
  delete obj;

  return fresh;
}

bool augmentDrain(TBeing* ch, spellNumT skill, bool heavy) {
  // Same shape as MetalRepair::OnDrain and WoodRepair::OnDrain: a flat cost
  // per pulse that skill partly offsets, plus the situational breaks.
  int add = (heavy && ch->getRace() == RACE_DWARF) ? 4 : 0;

  if (ch->homeTurf())
    add += 1;
  if (ch->backgroundBonus())
    add += 1;

  int base = heavy ? ::number(-10, -25) : ::number(-5, -15);
  int skillBack = ::number(1, max(1, ch->getSkillValue(skill) / 10));

  ch->addToMove(min(-1, base + skillBack + add));

  if (ch->getMove() < 10) {
    act("You are much too tired to keep at it.", false, ch, 0, 0, TO_CHAR);
    act("$n stops working, and wipes the sweat from $s brow.", true, ch, 0, 0,
      TO_ROOM);
    return true;
  }

  return false;
}

int getAugmentTickAmount(const TBeing* ch, spellNumT skill) {
  // A master works three points of structure per landed pulse, a novice one.
  // Without this the clock is the item's full structure in pulses, which is a
  // long sit for a breastplate.
  return max(1, ch->getSkillValue(skill) / 33);
}

Tier getMaxTierForMaterial(unsigned short material) {
  // Thresholds sit on natural gaps in material_nums[].hardness -- see the
  // hardness table in the design spec. Hardness is what a material will hold,
  // so this caps the ladder rather than choosing a tier.
  short hardness = material_nums[material].hardness;

  if (hardness >= 70)
    return Tier_Heavy;
  if (hardness >= 50)
    return Tier_Medium;
  if (hardness >= 20)
    return Tier_Light;
  return Tier_Clothing;
}

void stripFinish(TBeing* ch, TObj* obj) {
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(obj);
  if (!clothing)
    return;

  Tier tier = getWearableTier(clothing);
  Tier target = getTierBelow(tier);
  if (target == Tier_Max) {
    ch->sendTo("There is nothing left to cut away.\n\r");
    return;
  }

  obj->remObjStat(getTierRungFlags(tier));

  // getTier() infers restrictions the flags do not carry -- a monk cannot use
  // a heavy item whatever its flags say -- so clearing the pair does not
  // always move the tier. When it does not, the item would lose AC and gain
  // nothing, so put the flags back.
  if (getWearableTier(clothing) != target) {
    obj->addObjStat(getTierRungFlags(tier));
    act("You cannot find a seam in $p that would give.", false, ch, obj, 0,
      TO_CHAR);
    return;
  }

  double level = clothing->armorLevel(ARMOR_LEV_REAL) *
                 (getTierLoadLevel(target) / getTierLoadLevel(tier));

  // The bottom rung crosses a C++ type boundary: light armor demoted to
  // clothing has to become a TWorn, which means a new object off the clothing
  // template for this slot.
  if (target == Tier_Clothing && obj->itemType() == ITEM_ARMOR) {
    TObj* worn = convertWearableType(ch, obj, ITEM_WORN);
    if (!worn) {
      obj->addObjStat(getTierRungFlags(tier));
      act("$p will not come apart cleanly, and you leave it whole.", false, ch,
        obj, 0, TO_CHAR);
      return;
    }
    obj = worn;
    clothing = dynamic_cast<TBaseClothing*>(worn);
    if (!clothing)
      return;
  }

  // Sets the APPLY_ARMOR modifier and both structure values together, and
  // truncates on the way in. That truncation is the whole cost of a
  // conversion: strip a piece and plate it back and it does not return.
  clothing->setDefArmorLevel(static_cast<float>(level));

  act("You cut $p down to something a good deal less demanding.", false, ch,
    obj, 0, TO_CHAR);
  act("$n finishes cutting away at $p.", true, ch, obj, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_STRIP, obj);
}

void plateFinish(TBeing* ch, TObj* obj) {
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(obj);
  if (!clothing)
    return;

  Tier tier = getWearableTier(clothing);
  Tier target = getTierAbove(tier);
  if (target == Tier_Max) {
    ch->sendTo("There is no heavier thing for it to become.\n\r");
    return;
  }

  // Re-checked here and not only at the start: a mage can transmute the piece
  // mid-task, in either direction.
  if (target > getMaxTierForMaterial(obj->getMaterial())) {
    act("$p will not hold a heavier shape than it already has.", false, ch, obj,
      0, TO_CHAR);
    return;
  }

  obj->addObjStat(getTierRungFlags(target));

  if (getWearableTier(clothing) != target) {
    obj->remObjStat(getTierRungFlags(target));
    act("The plates will not sit right on $p.", false, ch, obj, 0, TO_CHAR);
    return;
  }

  // Plate adds value, so unlike Strip it answers to the skill ceiling:
  // promotion
  // rescales the item's own level, then stops at what a smith can reach. A
  // piece already above the ceiling keeps what it has and simply changes tier.
  double level = clothing->armorLevel(ARMOR_LEV_REAL) *
                 (getTierLoadLevel(target) / getTierLoadLevel(tier));
  level = min(level, getTierSkillMax(target));

  // The bottom rung crosses a C++ type boundary in the other direction:
  // clothing promoted to light armor has to become a TArmor.
  if (tier == Tier_Clothing && obj->itemType() == ITEM_WORN) {
    TObj* armor = convertWearableType(ch, obj, ITEM_ARMOR);
    if (!armor) {
      obj->remObjStat(getTierRungFlags(target));
      act("$p will not take a frame, and you leave it as it is.", false, ch,
        obj, 0, TO_CHAR);
      return;
    }
    obj = armor;
    clothing = dynamic_cast<TBaseClothing*>(armor);
    if (!clothing)
      return;
  }

  clothing->setDefArmorLevel(static_cast<float>(level));

  act("You work $p into something that will turn a heavier blow.", false, ch,
    obj, 0, TO_CHAR);
  act("$n finishes working at $p.", true, ch, obj, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_PLATE, obj);
}

// The soft end of the material table. Every one of these reads under the
// hardness threshold that caps an item at clothing, which is the only tier
// Sew produces.
bool isSewableMaterial(unsigned short material) {
  switch (material) {
    case MAT_CLOTH:
    case MAT_SILK:
    case MAT_WOOL:
    case MAT_FUR:
    case MAT_HEMP:
      return true;
    default:
      return false;
  }
}

TemplateSlot getTemplateSlotFromName(const sstring& name) {
  static const struct {
      const char* name;
      TemplateSlot slot;
  } names[] = {
    {"head", TemplateSlot::Head},
    {"neck", TemplateSlot::Neck},
    {"body", TemplateSlot::Body},
    {"back", TemplateSlot::Back},
    {"arm", TemplateSlot::Arm},
    {"wrist", TemplateSlot::Wrist},
    {"hand", TemplateSlot::Hand},
    {"waist", TemplateSlot::Waist},
    {"leg", TemplateSlot::Leg},
    {"foot", TemplateSlot::Foot},
  };

  sstring want = name.lower();
  for (const auto& entry : names)
    if (want == entry.name)
      return entry.slot;

  return TemplateSlot::COUNT;
}

race_t getRaceFromName(const sstring& name) {
  sstring want = name.lower();

  for (int i = 0; i < MAX_RACIAL_TYPES; i++) {
    if (!RaceNames[i])
      continue;
    if (want == sstring(RaceNames[i]).lower())
      return static_cast<race_t>(i);
  }

  return RACE_NORACE;
}

int getMaterialFromName(const sstring& name) {
  sstring want = name.lower();

  for (int i = 0; i < 200; i++) {
    if (!material_nums[i].mat_name[0])
      continue;
    if (want == sstring(material_nums[i].mat_name).lower())
      return i;
  }

  return -1;
}

int getSewUnits(const TBeing* ch, float weight) {
  // Commodities carry ten units per point of weight, so the piece's weight
  // converts directly. Skill buys back the waste, the way keycut's does.
  int base = max(1, static_cast<int>(weight * 10.0f));
  int waste = 150 - ch->getSkillValue(SKILL_SEW);

  return max(1, base * waste / 100);
}

double getSewLevelMax(const TBeing* ch) {
  // A woven piece has no prior level to rescale, so it is built from the
  // clothing ceiling down: full level at 35 and a maxed skill, less for either
  // shortfall. Sew adds value, so the ceiling applies.
  double levelPart = min(1.0, ch->GetMaxLevel() / 35.0);
  double skillPart = ch->getSkillValue(SKILL_SEW) / 100.0;

  return getTierSkillMax(Tier_Clothing) * levelPart * skillPart;
}

TCommodity* findCommodity(TBeing* ch, unsigned short material) {
  for (StuffIter it = ch->stuff.begin(); it != ch->stuff.end(); ++it) {
    TCommodity* tc = dynamic_cast<TCommodity*>(*it);
    if (tc && tc->getMaterial() == material)
      return tc;
  }

  return nullptr;
}

bool consumeCommodity(TBeing* ch, unsigned short material, int units) {
  TCommodity* commod = findCommodity(ch, material);
  if (!commod || commod->numUnits() < units)
    return false;

  commod->setWeight(commod->getWeight() - (units / 10.0));
  if (commod->numUnits() <= 0)
    delete commod;

  return true;
}

void sewFinish(TBeing* ch, TObj* obj) {
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(obj);
  if (!clothing)
    return;

  // The piece was built at its target level when the task started -- that is
  // what set the structure the clock counted down. Nothing is left to compute
  // here; the work is simply done.
  act("You work the last of the thread into $p.", false, ch, obj, 0, TO_CHAR);
  act("$n finishes sewing $p.", true, ch, obj, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_SEW, obj);
}

unsigned int allTierFlags() {
  return getTierRungFlags(Tier_Heavy) | getTierRungFlags(Tier_Medium) |
         getTierRungFlags(Tier_Light);
}

void halveArmorValues(TBaseClothing* clothing) {
  if (!clothing)
    return;

  // The AC affect's modifier is the negative of the item's armor value, so
  // halving it toward zero is what weakens the piece.
  for (int i = 0; i < MAX_OBJ_AFFECT; i++)
    if (clothing->affected[i].location == APPLY_ARMOR)
      clothing->affected[i].modifier /= 2;

  short points = max(1, clothing->getMaxStructPoints() / 2);
  clothing->setMaxStructPoints(points);
  clothing->setStructPoints(min(clothing->getStructPoints(), points));
}

void bangleFinish(TBeing* ch, TObj* obj) {
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(obj);
  if (!clothing)
    return;

  Tier tier = getWearableTier(clothing);
  if (tier == Tier_Jewelry || tier == Tier_Max) {
    ch->sendTo("There is nothing there left to work.\n\r");
    return;
  }

  // Jewelry is priced at the clothing rate, so a piece coming down from a
  // higher tier rescales onto that scale first -- the same ratio move Strip
  // makes, without the flag work, since jewelry's tier does not come from
  // flags at all.
  double level = clothing->armorLevel(ARMOR_LEV_REAL) *
                 (getTierLoadLevel(Tier_Clothing) / getTierLoadLevel(tier));

  TObj* jewel = convertWearableType(ch, obj, ITEM_JEWELRY);
  if (!jewel) {
    act("$p will not take the shape you want of it.", false, ch, obj, 0,
      TO_CHAR);
    return;
  }

  TBaseClothing* worked = dynamic_cast<TBaseClothing*>(jewel);
  if (!worked)
    return;

  // Anyone may wear jewelry: the anti-class flags come off with the tier they
  // used to mean.
  jewel->remObjStat(allTierFlags());

  worked->setDefArmorLevel(static_cast<float>(level));

  // Then the convention: half the AC and structure of a wearable at this
  // level, against twice the stat allowance. The stat affects themselves came
  // across untouched -- they are what Distill is for.
  halveArmorValues(worked);

  act("You work $p down into something small and ornamental.", false, ch, jewel,
    0, TO_CHAR);
  act("$n finishes working $p into something ornamental.", true, ch, jewel, 0,
    TO_ROOM);

  augmentTaskExp(ch, SKILL_BANGLE, jewel);
}

bool isMetalMaterial(unsigned short material) {
  return material >= 150 && material < 150 + MAX_MAT_METAL;
}

int getSmeltQuality(int hits, int misses) {
  int total = hits + misses;
  if (total <= 0)
    return 1;

  int percent = hits * 100 / total;

  if (percent >= 90)
    return 5;
  if (percent >= 80)
    return 4;
  if (percent >= 65)
    return 3;
  if (percent >= 45)
    return 2;
  return 1;
}

int getSmeltUnits(const TBeing* ch, float weight) {
  // Commodities and ingots both run at ten units per point of weight. Half the
  // metal is lost to the crucible; a smith who has gone deep into advanced
  // blacksmithing loses only a quarter.
  bool specialized = ch->getSkillValue(SKILL_BLACKSMITHING_ADVANCED) >= 50;
  double recovery = specialized ? 0.75 : 0.5;

  return max(1, static_cast<int>(weight * 10.0 * recovery));
}

void smeltFinish(TBeing* ch, TObj* obj, int hits, int misses) {
  TIngot* ingot = dynamic_cast<TIngot*>(read_object(kIngotVnum, VIRTUAL));
  if (!ingot) {
    act("$p sits in the fire, and nothing comes of it.", false, ch, obj, 0,
      TO_CHAR);
    return;
  }

  ingot->swapToStrung();

  int quality = getSmeltQuality(hits, misses);
  int units = getSmeltUnits(ch, obj->getWeight());

  ingot->setIngotQuality(quality);
  ingot->setIngotUnits(units);
  ingot->setMaterial(obj->getMaterial());
  ingot->setWeight(units / 10.0);

  // Volume is what Forge measures a piece of gear against, so the bar has to
  // carry an honest one rather than the prototype's placeholder: the same
  // weight of mithril and of iron are very different amounts of metal.
  ingot->setVolume(volumeForWeight(units / 10.0f, obj->getMaterial()));

  // How much work this bar is, which becomes the clock for anything done to
  // it: the metal's own structureMod scaled by how much of it there is.
  ingot->setMaxStructPoints(getIngotStructure(obj->getMaterial(), units));
  ingot->setStructPoints(ingot->getMaxStructPoints());

  sstring metal = material_nums[obj->getMaterial()].mat_name;
  ingot->name = format("ingot %s metal") % metal;
  ingot->shortDescr = format("an ingot of %s") % metal;
  ingot->setDescr(format("An ingot of %s lies here.") % metal);

  // The stats come across, the armor value does not: AC belongs to the shape
  // of a thing, and the shape is what just went into the fire. Forge projects
  // its own from the level it can reach.
  //
  // A paired item carried double the stats of a single one -- that is what
  // being a pair meant -- so what survives it is half.
  bool paired = obj->isObjStat(ITEM_PAIRED);
  for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
    ingot->affected[i] = obj->affected[i];

    if (ingot->affected[i].location == APPLY_ARMOR) {
      ingot->affected[i].location = APPLY_NONE;
      ingot->affected[i].modifier = 0;
      continue;
    }

    if (paired)
      ingot->affected[i].modifier /= 2;
  }

  --(*obj);
  delete obj;

  *ch += *ingot;

  act("You pour off $p, and the rest is slag.", false, ch, ingot, 0, TO_CHAR);
  act("$n pours off $p.", true, ch, ingot, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_SMELT, ingot);
}

int getMetalDifficulty(unsigned short material) {
  const MetalMaterial* metal = findMetalMaterial(material);
  return metal ? metal->difficultyMod : 0;
}

int getMetalStructure(unsigned short material) {
  const MetalMaterial* metal = findMetalMaterial(material);
  if (metal)
    return metal->structureMod;

  // Ten of the twenty-eight metals are not in the tiered table. Hardness is
  // the one number every material has, and it runs on a comparable scale.
  return max(1, static_cast<int>(material_nums[material].hardness));
}

int getForgeRollMod(const TBeing* ch, unsigned short material) {
  // A point of help per five learned in advanced blacksmithing, against the
  // metal's own resistance. Common stock reads -10 in the table -- easier than
  // baseline -- so a specialist working tin is a long way ahead of one
  // wrestling adamantite.
  int help = ch->getSkillValue(SKILL_BLACKSMITHING_ADVANCED) / 5;

  return help - getMetalDifficulty(material);
}

int getIngotStructure(unsigned short material, int units) {
  return max(1, getMetalStructure(material) * max(0, units) / 100);
}

bool hasApplies(const TObj* obj) {
  if (!obj)
    return false;

  for (int i = 0; i < MAX_OBJ_AFFECT; i++)
    if (obj->affected[i].location != APPLY_NONE && obj->affected[i].modifier)
      return true;

  return false;
}

int getCombineQuality(int q1, int u1, int q2, int u2) {
  int total = u1 + u2;
  if (total <= 0)
    return q1;

  // Weighted by size, truncated: a half point of improvement is not
  // improvement. The floor at q1 is what stops a big dirty bar from dragging
  // down a small clean one.
  int blended = (q1 * u1 + q2 * u2) / total;

  return max(q1, min(5, blended));
}

void combineFinish(TBeing* ch, TObj* target, TObj* donor, int hits,
  int misses) {
  TIngot* into = dynamic_cast<TIngot*>(target);
  TIngot* from = dynamic_cast<TIngot*>(donor);
  if (!into || !from)
    return;

  int blended = getCombineQuality(into->getIngotQuality(), into->getIngotUnits(),
    from->getIngotQuality(), from->getIngotUnits());

  // Workmanship caps the result: the grade of the combine itself is read on
  // the same thresholds a smelt uses, and the bar cannot come out better than
  // the work that made it. It still never drops below what it already was.
  int workmanship = getSmeltQuality(hits, misses);
  int quality = max(into->getIngotQuality(), min(blended, workmanship));

  int units = into->getIngotUnits() + from->getIngotUnits();

  into->setIngotQuality(quality);
  into->setIngotUnits(units);
  into->setWeight(units / 10.0);
  into->setVolume(volumeForWeight(units / 10.0f, into->getMaterial()));
  into->setMaxStructPoints(getIngotStructure(into->getMaterial(), units));
  into->setStructPoints(into->getMaxStructPoints());

  --(*from);
  delete from;

  act("You beat the two together into $p.", false, ch, into, 0, TO_CHAR);
  act("$n beats two bars together into $p.", true, ch, into, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_FORGE, into);
}

Tier getTierFromName(const sstring& name) {
  sstring want = name.lower();

  if (want == "clothing")
    return Tier_Clothing;
  if (want == "light")
    return Tier_Light;
  if (want == "medium")
    return Tier_Medium;
  if (want == "heavy")
    return Tier_Heavy;

  return Tier_Max;
}

unsigned int getTierFlags(Tier tier) {
  // The rungs nest: light keeps mages and shamans out, medium adds monks and
  // thieves, heavy adds clerics and rangers on top of both.
  unsigned int flags = 0;

  if (tier >= Tier_Light)
    flags |= getTierRungFlags(Tier_Light);
  if (tier >= Tier_Medium)
    flags |= getTierRungFlags(Tier_Medium);
  if (tier >= Tier_Heavy)
    flags |= getTierRungFlags(Tier_Heavy);

  return flags;
}

double getForgeLevelMax(const TBeing* ch, Tier tier) {
  double levelPart = min(1.0, ch->GetMaxLevel() / 35.0);
  double skillPart = ch->getSkillValue(SKILL_FORGE) / 100.0;

  return getTierSkillMax(tier) * levelPart * skillPart;
}

void reduceOneApply(TObj* obj) {
  if (!obj)
    return;

  // AC is not in this list: it comes off with the level penalty at the end,
  // and taking it here would charge for the same miss twice.
  int candidates[MAX_OBJ_AFFECT];
  int found = 0;

  for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
    if (obj->affected[i].location == APPLY_NONE ||
        obj->affected[i].location == APPLY_ARMOR)
      continue;
    if (!obj->affected[i].modifier)
      continue;
    candidates[found++] = i;
  }

  if (!found)
    return;

  int which = candidates[::number(0, found - 1)];

  // Degrading means moving the number the way the wearer would not want it.
  // For almost everything that is down. Noise is the exception: it measures
  // how much sound the wearer makes, so a negative value is the good one --
  // see the -40 blessing in cmd_egotrip.cc -- and taking a point off a piece
  // means moving noise up.
  if (obj->affected[which].location == APPLY_NOISE)
    obj->affected[which].modifier++;
  else
    obj->affected[which].modifier--;

  // An apply worn down to nothing is simply gone.
  if (!obj->affected[which].modifier)
    obj->affected[which].location = APPLY_NONE;
}

void spoilLeftoverMetal(TBeing* ch, const char* ingotName, int amount) {
  if (!ingotName || !*ingotName || amount <= 0)
    return;

  TIngot* bar =
    dynamic_cast<TIngot*>(searchLinkedListVis(ch, ingotName, ch->stuff));
  if (!bar)
    return;

  int left = bar->getIngotUnits() - amount;

  if (left <= 0) {
    act("The last of $p is spoiled along with it.", false, ch, bar, 0, TO_CHAR);
    --(*bar);
    delete bar;
    return;
  }

  bar->setIngotUnits(left);
  bar->setWeight(left / 10.0);
  bar->setVolume(volumeForWeight(left / 10.0f, bar->getMaterial()));
  bar->setMaxStructPoints(getIngotStructure(bar->getMaterial(), left));
  bar->setStructPoints(min(bar->getStructPoints(), bar->getMaxStructPoints()));
}

void spoilLeftoverThread(TBeing* ch, const char* skeinName, int amount) {
  if (!skeinName || !*skeinName || amount <= 0)
    return;

  TSkein* skein =
    dynamic_cast<TSkein*>(searchLinkedListVis(ch, skeinName, ch->stuff));
  if (!skein)
    return;

  int left = skein->getSkeinUnits() - amount;

  if (left <= 0) {
    act("The last of $p is spoiled along with it.", false, ch, skein, 0,
      TO_CHAR);
    --(*skein);
    delete skein;
    return;
  }

  skein->setSkeinUnits(left);
  skein->setWeight(left / 10.0);
  skein->setVolume(volumeForWeight(left / 10.0f, skein->getMaterial()));
  skein->setMaxStructPoints(getSkeinStructure(skein->getMaterial(), left));
  skein->setStructPoints(
    min(skein->getStructPoints(), skein->getMaxStructPoints()));
}

void forgeFinish(TBeing* ch, TObj* obj, int penalty) {
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(obj);
  if (!clothing)
    return;

  // The piece was made at its projection; what the misses took comes off here,
  // so AC and structure fall together with the stats that bled out along the
  // way.
  double level = clothing->armorLevel(ARMOR_LEV_REAL) *
                 max(0.0, (100.0 - min(penalty, 95)) / 100.0);

  clothing->setDefArmorLevel(static_cast<float>(level));

  if (penalty > 0)
    act("You finish $p, knowing it could have gone better.", false, ch, obj, 0,
      TO_CHAR);
  else
    act("You finish $p, and it is exactly what you meant it to be.", false, ch,
      obj, 0, TO_CHAR);

  act("$n finishes work on $p.", true, ch, obj, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_FORGE, obj);
}

// Carats convert evenly onto the ten Soulstone Levels: five carats to a Level,
// clamped at both ends. The world's opals run 0 to 50 carats, so most land
// between 1 and 3 and only the two rarest stones reach 10. A stone with no
// carats at all still becomes a Level 1 soulstone.
int getSoulLevelForCarats(int carats) {
  return max(1, min(10, carats / 5));
}

// Ten landed pulses per carat. A tiny opal is quick work; a fifty-carat stone
// is five hundred successes, and the lifeforce draw scales with carats on top
// of that.
int getEnsoulLength(int carats) { return max(10, carats * 10); }

void ensoulFinish(TBeing* ch, TObj* obj, int carats) {
  TSoulstone* stone =
    dynamic_cast<TSoulstone*>(read_object(kSoulstoneVnum, VIRTUAL));
  if (!stone) {
    act("$p goes dark, and nothing answers.", false, ch, obj, 0, TO_CHAR);
    return;
  }

  stone->swapToStrung();
  stone->setSoulLevel(getSoulLevelForCarats(carats));
  stone->setCharges(0);

  // All ten Levels share one vnum, so the Level has to live in the name for a
  // shaman to tell two stones apart at a glance.
  stone->name = format("soulstone stone soul level%d") % stone->getSoulLevel();
  stone->shortDescr = format("a soulstone of level %d") % stone->getSoulLevel();
  stone->setDescr(
    format("A soulstone of level %d lies here.") % stone->getSoulLevel());

  augmentTaskExp(ch, SKILL_ENSOUL, obj);

  --(*obj);
  delete obj;

  *ch += *stone;

  act("The stone answers, and $p is a soulstone now.", false, ch, stone, 0,
    TO_CHAR);
  act("Something in $n's hands goes quiet, and stays quiet.", true, ch, stone,
    0, TO_ROOM);
}

void TBeing::doEnsoul(const char* argument) {
  char name_buf[256];

  if (!doesKnowSkill(SKILL_ENSOUL)) {
    sendTo("You know nothing about calling a soul into a stone.\n\r");
    return;
  }

  strcpy(name_buf, argument);

  if (!*name_buf) {
    sendTo("What is it you intend to ensoul?\n\r");
    return;
  }

  TThing* found = searchLinkedListVis(this, name_buf, stuff);
  TOpal* opal = dynamic_cast<TOpal*>(found);
  if (!opal) {
    sendTo("You need a powerstone in hand for that.\n\r");
    return;
  }

  if (task)
    stopTask();

  int carats = max(0, opal->psGetCarats());

  act("You take up $p and begin calling.", false, this, opal, 0, TO_CHAR);
  act("$n takes up $p and begins to call.", true, this, opal, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_ENSOUL, 8);

  // The clock is ten successes per carat, and the carat count rides in status
  // so the pulse can scale its lifeforce draw by it.
  start_task(this, opal, nullptr, TASK_ENSOUL, "", getEnsoulLength(carats),
    in_room, min(255, carats), 0, 0);
}

bool corpseIsPristine(const TObj* corpse) {
  const TBaseCorpse* body = dynamic_cast<const TBaseCorpse*>(corpse);
  if (!body)
    return false;

  // NO_REGEN marks a severed limb rather than a body, and a limb has no soul
  // to call out of it.
  if (body->isCorpseFlag(CORPSE_NO_REGEN))
    return false;

  unsigned int opened = CORPSE_NO_SKIN | CORPSE_HALF_SKIN | CORPSE_PC_SKINNING |
                        CORPSE_NO_BUTCHER | CORPSE_HALF_BUTCHERED |
                        CORPSE_PC_BUTCHERING | CORPSE_NO_DISSECT |
                        CORPSE_NO_RITES;

  return !body->isCorpseFlag(opened);
}

TVial* findHolyWater(TBeing* ch) {
  for (StuffIter it = ch->stuff.begin(); it != ch->stuff.end(); ++it) {
    TVial* vial = dynamic_cast<TVial*>(*it);
    if (vial && vial->getDrinkType() == LIQ_HOLYWATER &&
        vial->getDrinkUnits() > 0)
      return vial;
  }

  return nullptr;
}

TSoulstone* findSoulstone(TBeing* ch) {
  for (StuffIter it = ch->stuff.begin(); it != ch->stuff.end(); ++it) {
    TSoulstone* stone = dynamic_cast<TSoulstone*>(*it);
    if (stone)
      return stone;
  }

  return nullptr;
}

int getRitesYield(int soulLevel, int corpseLevel) {
  return max(1, soulLevel * corpseLevel / 10);
}

void ritesFinish(TBeing* ch, TObj* corpse) {
  TBaseCorpse* body = dynamic_cast<TBaseCorpse*>(corpse);
  TSoulstone* stone = findSoulstone(ch);

  if (!body || !stone) {
    ch->sendTo("The call goes out and finds nothing to hold it.\n\r");
    return;
  }

  int gained = getRitesYield(stone->getSoulLevel(),
    static_cast<int>(body->getCorpseLevel()));

  bool deepened = stone->addCharges(gained);

  // The soul was the last thing worth taking. Everything else that would have
  // opened this body is closed off, and sacrifice is told there is nothing
  // here to give.
  body->addCorpseFlag(CORPSE_NO_SKIN | CORPSE_NO_BUTCHER | CORPSE_NO_DISSECT |
                      CORPSE_NO_RITES);

  act("The last of $p goes out of it and into the stone.", false, ch, corpse, 0,
    TO_CHAR);
  act("$n lowers $s hands, and $p lies still.", true, ch, corpse, 0, TO_ROOM);

  ch->sendTo(format("Your soulstone takes %d charge%s.\n\r") % gained %
             (gained == 1 ? "" : "s"));

  if (deepened) {
    // All ten Levels share one vnum, so the name has to be rewritten for the
    // change to be visible at all.
    stone->swapToStrung();
    stone->name =
      format("soulstone stone soul level%d") % stone->getSoulLevel();
    stone->shortDescr =
      format("a soulstone of level %d") % stone->getSoulLevel();
    stone->setDescr(
      format("A soulstone of level %d lies here.") % stone->getSoulLevel());

    act("$p deepens, and holds more than it did.", false, ch, stone, 0,
      TO_CHAR);
  }

  ch->gainTaskExp(SKILL_RITES,
    max(1, static_cast<int>(body->getCorpseLevel())),
    max(1.0, static_cast<double>(gained)), false);
}

void TBeing::doRites(const char* argument) {
  char name_buf[256];

  if (!doesKnowSkill(SKILL_RITES)) {
    sendTo("You know nothing of the rites for the dead.\n\r");
    return;
  }

  strcpy(name_buf, argument);

  if (!*name_buf) {
    sendTo("Over whom do you intend to say the rites?\n\r");
    return;
  }

  if (getPosition() > POSITION_SITTING) {
    sendTo("You cannot say the rites unless you are seated.\n\r");
    return;
  }

  // The symbol is held and beheld, the same as attuning one.
  TSymbol* symbol = dynamic_cast<TSymbol*>(equipment[getPrimaryHold()]);
  if (!symbol) {
    sendTo("You need your holy symbol in hand for that.\n\r");
    return;
  }

  if (!findHolyWater(this)) {
    sendTo("You have no holy water to say the rites with.\n\r");
    return;
  }

  TSoulstone* stone = findSoulstone(this);
  if (!stone) {
    sendTo("You have no soulstone to call the soul into.\n\r");
    return;
  }

  if (stone->getCharges() >= stone->chargeCap()) {
    act("$p will hold nothing more.", false, this, stone, 0, TO_CHAR);
    return;
  }

  // The body may be at your feet or in your arms.
  TObj* corpse =
    dynamic_cast<TObj*>(searchLinkedListVis(this, name_buf, stuff));
  if (!corpse && roomp)
    corpse = dynamic_cast<TObj*>(searchLinkedListVis(this, name_buf,
      roomp->stuff));

  if (!dynamic_cast<TBaseCorpse*>(corpse)) {
    sendTo("You see no corpse here by that name.\n\r");
    return;
  }

  if (!corpseIsPristine(corpse)) {
    act("$p has already been opened; there is nothing left in it to call.",
      false, this, corpse, 0, TO_CHAR);
    return;
  }

  if (task)
    stopTask();

  act("You take up your symbol and begin the rites over $p.", false, this,
    corpse, 0, TO_CHAR);
  act("$n takes up $s symbol and begins to pray over $p.", true, this, corpse,
    0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_RITES, 8);

  // A heavier soul takes longer to draw out, so the corpse's own level is the
  // clock. Nothing about the caster enters it.
  TBaseCorpse* body = dynamic_cast<TBaseCorpse*>(corpse);
  start_task(this, corpse, nullptr, TASK_RITES, "",
    max(10, static_cast<int>(body->getCorpseLevel())), in_room, 0, 0, 0);
}

double getBolsterMax(const TBeing* ch, Tier tier) {
  return getTierSkillMax(tier) * min(1.0, ch->GetMaxLevel() / 35.0);
}

namespace {
  // Seven bands over the fraction of the ceiling already reached. The names
  // are taskDiffT's, borrowed as vocabulary -- that enum carries no math of
  // its own, only display strings.
  struct BolsterBand {
      double upTo;  // fraction of ceiling, exclusive
      int mod;
      const char* name;
  };

  constexpr BolsterBand bolsterBands[] = {
    {0.30, 40, "Trivial"},
    {0.45, 25, "Easy"},
    {0.60, 10, "Normal"},
    {0.75, -10, "Difficult"},
    {0.85, -30, "Dangerous"},
    {0.95, -50, "Hopeless"},
    {99.0, -75, "Near-impossible"},
  };

  const BolsterBand& bolsterBandFor(double armorLevel, double ceiling) {
    double progress = (ceiling > 0.0) ? (armorLevel / ceiling) : 1.0;

    for (const auto& band : bolsterBands)
      if (progress < band.upTo)
        return band;

    return bolsterBands[6];
  }
}  // namespace

int getBolsterBandMod(double armorLevel, double ceiling) {
  return bolsterBandFor(armorLevel, ceiling).mod;
}

sstring getBolsterBandName(double armorLevel, double ceiling) {
  return bolsterBandFor(armorLevel, ceiling).name;
}

int getBolsterChargeCost(double armorLevel, int soulLevel) {
  // armorLevel is the 1-60 derived level, not the APPLY_ARMOR modifier, which
  // runs in the hundreds. Expressed as a multiplier over Soulstone Level
  // rather than a division by it: the other way collapses to zero charges
  // under integer arithmetic and makes high-tier work free.
  return max(1, static_cast<int>(armorLevel * 10) / max(1, soulLevel));
}

void TBeing::doBolster(const char* argument) {
  char name_buf[256];

  if (!doesKnowSkill(SKILL_BOLSTER)) {
    sendTo("You know nothing about bolstering armor.\n\r");
    return;
  }

  strcpy(name_buf, argument);

  if (!*name_buf) {
    sendTo("What is it you intend to bolster?\n\r");
    return;
  }

  TThing* found = searchLinkedListVis(this, name_buf, stuff);
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(found);
  TObj* obj = dynamic_cast<TObj*>(found);

  if (!clothing || !obj) {
    sendTo("You need to be carrying that to bolster it.\n\r");
    return;
  }

  TSoulstone* stone = findSoulstone(this);
  if (!stone) {
    sendTo("You have no soulstone to draw on.\n\r");
    return;
  }

  Tier tier = getWearableTier(clothing);
  double ceiling = getBolsterMax(this, tier);
  double level = clothing->armorLevel(ARMOR_LEV_REAL);

  if (level >= ceiling) {
    act("$p is already everything you could make of it.", false, this, obj, 0,
      TO_CHAR);
    return;
  }

  int cost = getBolsterChargeCost(level, stone->getSoulLevel());
  if (stone->getCharges() < cost) {
    sendTo(format("That would take %d charges an attempt, and your stone "
                  "holds %d.\n\r") %
           cost % stone->getCharges());
    return;
  }

  if (task)
    stopTask();

  sendTo(format("The work ahead of you looks %s.\n\r") %
         getBolsterBandName(level, ceiling).lower());

  act("You set to work on $p.", false, this, obj, 0, TO_CHAR);
  act("$n sets to work on $p.", true, this, obj, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_BOLSTER, 8);

  // No fixed clock: bolstering runs until the piece reaches its ceiling, the
  // stone runs dry, or something interrupts it -- the way sharpening simply
  // halts at maximum sharpness. timeLeft is a stop, not a countdown.
  start_task(this, obj, nullptr, TASK_BOLSTER, "", 1, in_room, 0, 0, 0);
}

int getDistillDeposit(const TBeing* ch, int modifier) {
  // Magnitude first, then skill, then the floor. A ring carrying +5 STR gives
  // five charges to a mage who has mastered the work and proportionally fewer
  // to one who has not -- but never nothing, so no distillation is wasted.
  int magnitude = abs(modifier);
  int scaled = magnitude * ch->getSkillValue(SKILL_DISTILL) / 100;

  return max(1, scaled);
}

TEssence* findEssence(TBeing* ch, int apply) {
  for (StuffIter it = ch->stuff.begin(); it != ch->stuff.end(); ++it) {
    TEssence* essence = dynamic_cast<TEssence*>(*it);
    if (essence && essence->getApplyType() == apply)
      return essence;
  }

  return nullptr;
}

TEssence* depositEssence(TBeing* ch, int apply, int charges) {
  TEssence* essence = findEssence(ch, apply);
  bool deepened = false;

  if (essence) {
    deepened = essence->addCharges(charges);
  } else {
    essence = dynamic_cast<TEssence*>(read_object(kEssenceVnum, VIRTUAL));
    if (!essence)
      return nullptr;

    essence->swapToStrung();
    essence->setApplyType(apply);
    essence->setQuality(1);
    essence->setCharges(0);
    essence->addCharges(charges);

    *ch += *essence;
  }

  const char* what = essenceApplyName(apply);

  // One vnum for every apply and Quality, so the name has to carry both or
  // two essences are indistinguishable in inventory.
  essence->swapToStrung();
  essence->name = format("essence %s") % what;
  essence->shortDescr =
    format("an essence of %s (quality %d)") % what % essence->getQuality();
  essence->setDescr(format("An essence of %s hangs in the air here.") % what);

  ch->sendTo(format("%d charge%s of %s essence.\n\r") % charges %
             (charges == 1 ? "" : "s") % what);

  if (deepened)
    act("$p deepens, and would write more than it did.", false, ch, essence, 0,
      TO_CHAR);

  return essence;
}

void distillFinish(TBeing* ch, TObj* obj) {
  int deposits = 0;

  // Every eligible affect deposits; nothing is chosen or discarded. A ring of
  // +3 STR and +2 DEX yields both.
  for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
    int apply = obj->affected[i].location;

    if (!isEssenceApply(apply) || !obj->affected[i].modifier)
      continue;

    if (depositEssence(ch, apply, getDistillDeposit(ch, obj->affected[i].modifier)))
      deposits++;
  }

  if (!deposits)
    act("$p comes apart, and nothing of it was worth keeping.", false, ch, obj,
      0, TO_CHAR);
  else
    act("$p comes apart, and what it carried is yours.", false, ch, obj, 0,
      TO_CHAR);

  act("$p comes apart in $n's hands.", true, ch, obj, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_DISTILL, obj);

  --(*obj);
  delete obj;
}

void TBeing::doDistill(const char* argument) {
  char name_buf[256];

  if (!doesKnowSkill(SKILL_DISTILL)) {
    sendTo("You know nothing about drawing the virtue out of a thing.\n\r");
    return;
  }

  strcpy(name_buf, argument);

  if (!*name_buf) {
    sendTo("What is it you intend to distill?\n\r");
    return;
  }

  TThing* found = searchLinkedListVis(this, name_buf, stuff);
  TObj* obj = dynamic_cast<TObj*>(found);
  if (!obj) {
    sendTo("You need to be carrying that to distill it.\n\r");
    return;
  }

  // Jewelry only, which puts Bangle in front of every essence: scrap has to be
  // transmuted, forged into something wearable and bangled before a mage can
  // draw anything out of it. That road is long on purpose -- it is what pays
  // for the stats an offcut carries out of a resize.
  if (obj->itemType() != ITEM_JEWELRY) {
    act("$p is not something you can draw virtue out of. Bangle it first.",
      false, this, obj, 0, TO_CHAR);
    return;
  }

  bool anything = false;
  for (int i = 0; i < MAX_OBJ_AFFECT; i++)
    if (isEssenceApply(obj->affected[i].location) && obj->affected[i].modifier)
      anything = true;

  if (!anything) {
    act("$p carries nothing an essence could hold.", false, this, obj, 0,
      TO_CHAR);
    return;
  }

  if (task)
    stopTask();

  act("You begin drawing the virtue out of $p.", false, this, obj, 0, TO_CHAR);
  act("$n begins working over $p.", true, this, obj, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_DISTILL, 8);

  start_task(this, obj, nullptr, TASK_DISTILL, "",
    max(1, static_cast<int>(obj->getMaxStructPoints())), in_room, 0, 0, 0);
}

void copyStatApplies(const TObj* from, TObj* to) {
  if (!from || !to)
    return;

  int slot = 0;

  for (int i = 0; i < MAX_OBJ_AFFECT && slot < MAX_OBJ_AFFECT; i++) {
    if (from->affected[i].location == APPLY_NONE ||
        from->affected[i].location == APPLY_ARMOR)
      continue;
    if (!from->affected[i].modifier)
      continue;

    to->affected[slot] = from->affected[i];
    slot++;
  }
}

TObj* makeOffcut(TBeing* ch, TObj* obj, int leftover) {
  if (leftover <= 0)
    return nullptr;

  TObj* scrap = read_object(kOffcutVnum, VIRTUAL);
  if (!scrap)
    return nullptr;

  scrap->swapToStrung();
  scrap->setMaterial(obj->getMaterial());
  scrap->setVolume(leftover);
  scrap->setWeight(weightForVolume(leftover, obj->getMaterial()));

  // Offcuts do not rot away. The material is the point of them, and a player
  // who sets one aside to work later should find it where they left it.
  scrap->obj_flags.decay_time = OBJ_NOTIMER;

  copyStatApplies(obj, scrap);

  sstring what = material_nums[obj->getMaterial()].mat_name;
  scrap->name = format("offcut scrap %s") % what;
  scrap->shortDescr = format("a piece of %s scrap") % what;
  scrap->setDescr(format("A piece of %s scrap lies here.") % what);

  *ch += *scrap;

  return scrap;
}

TObj* convertWearableSlot(TBeing* ch, TObj* obj, TemplateSlot slot) {
  if (slot == TemplateSlot::COUNT)
    return nullptr;

  TObj* fresh = makeBlankWearable(obj->itemType(), slot);
  if (!fresh)
    return nullptr;

  // The template owns the wear flags, which is the whole point here: they are
  // what rent rebuilds from the vnum, so the new slot has to come from a
  // prototype that already has it.
  unsigned int wearFlags = fresh->obj_flags.wear_flags;
  fresh->obj_flags = obj->obj_flags;
  fresh->obj_flags.wear_flags = wearFlags;
  fresh->addObjStat(ITEM_STRUNG);

  for (int i = 0; i < MAX_OBJ_AFFECT; i++)
    fresh->affected[i] = obj->affected[i];

  fresh->name = obj->name;
  fresh->shortDescr = obj->shortDescr;
  fresh->setDescr(obj->getDescr());
  fresh->action_description = obj->action_description;
  fresh->setMaterial(obj->getMaterial());
  fresh->setWeight(obj->getWeight());
  fresh->canBeSeen = obj->canBeSeen;

  --(*obj);
  *ch += *fresh;
  delete obj;

  return fresh;
}

bool isOffcut(const TObj* obj) {
  return obj && obj->objVnum() == kOffcutVnum;
}

void resizeFinish(TBeing* ch, TObj* obj, race_t race) {
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(obj);
  if (!clothing)
    return;

  TemplateSlot slot = getWearableSlot(obj);
  int wanted = getSlotVolumeForRace(slot, race);
  int had = obj->getVolume();

  if (wanted <= 0) {
    act("You cannot picture $p on a body that shape.", false, ch, obj, 0,
      TO_CHAR);
    return;
  }

  obj->setVolume(wanted);
  obj->setWeight(weightForVolume(wanted, obj->getMaterial()));

  const char* sizeName = raceSizeName(race);
  if (sizeName)
    ch->sendTo(format("You work it down to %s size.\n\r") % sizeName);

  // Metal cut away does not vanish. It comes off as an offcut carrying what
  // the piece carried -- the stats, never the AC -- and has to go back through
  // the crucible before it can be worked into anything.
  int leftover = had - wanted;
  if (leftover <= 0) {
    act("You finish reworking $p.", false, ch, obj, 0, TO_CHAR);
    act("$n finishes reworking $p.", true, ch, obj, 0, TO_ROOM);
    return;
  }

  if (!makeOffcut(ch, obj, leftover)) {
    act("You finish reworking $p, and sweep the scrap away.", false, ch, obj, 0,
      TO_CHAR);
    return;
  }

  act("You finish reworking $p, and set the offcut aside.", false, ch, obj, 0,
    TO_CHAR);
  act("$n finishes reworking $p.", true, ch, obj, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_FORGE, obj);
}

void TBeing::doForgeResize(const char* argument) {
  sstring args(argument);
  sstring itemName = args.word(1);
  sstring raceName = args.word(2);

  if (itemName.empty() || raceName.empty()) {
    sendTo("Resize what, for whom?\n\r");
    sendTo("Syntax: forge resize <item> <race>\n\r");
    return;
  }

  TThing* found = searchLinkedListVis(this, itemName.c_str(), stuff);
  TObj* obj = dynamic_cast<TObj*>(found);
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(found);

  if (!obj || !clothing) {
    sendTo("You need to be carrying that to resize it.\n\r");
    return;
  }

  if (!isMetalMaterial(obj->getMaterial())) {
    act("$p is not metal. That is work for a tailor.", false, this, obj, 0,
      TO_CHAR);
    return;
  }

  TemplateSlot slot = getWearableSlot(obj);
  if (slot == TemplateSlot::COUNT) {
    act("You cannot make sense of how $p is meant to be worn.", false, this,
      obj, 0, TO_CHAR);
    return;
  }

  race_t race = getRaceFromName(raceName);
  if (race == RACE_NORACE) {
    sendTo(format("You've never seen a %s to fit armor to.\n\r") % raceName);
    return;
  }

  int wanted = getSlotVolumeForRace(slot, race);
  if (wanted <= 0) {
    sendTo(format("You have no pattern that would fit a %s.\n\r") % raceName);
    return;
  }

  if (wanted == obj->getVolume()) {
    act("$p is already that size.", false, this, obj, 0, TO_CHAR);
    return;
  }

  // Growing a piece needs metal from somewhere, and the only place metal comes
  // from is a bar. Shrinking one leaves metal over instead, which is the
  // offcut.
  if (wanted > obj->getVolume()) {
    int needUnits = max(1, static_cast<int>(
      weightForVolume(wanted - obj->getVolume(), obj->getMaterial()) * 10.0f));

    TIngot* bar = nullptr;
    for (StuffIter it = stuff.begin(); it != stuff.end(); ++it) {
      TIngot* candidate = dynamic_cast<TIngot*>(*it);
      if (candidate && candidate->getMaterial() == obj->getMaterial() &&
          candidate->getIngotUnits() >= needUnits) {
        bar = candidate;
        break;
      }
    }

    if (!bar) {
      sendTo(format("Making $p that much bigger needs %d units of %s, and you "
                    "have no bar with that much in it.\n\r") %
             needUnits % material_nums[obj->getMaterial()].mat_name);
      return;
    }

    int left = bar->getIngotUnits() - needUnits;
    if (left <= 0) {
      --(*bar);
      delete bar;
    } else {
      bar->setIngotUnits(left);
      bar->setWeight(left / 10.0);
      bar->setVolume(volumeForWeight(left / 10.0f, bar->getMaterial()));
      bar->setMaxStructPoints(getIngotStructure(bar->getMaterial(), left));
      bar->setStructPoints(bar->getMaxStructPoints());
    }
  }

  if (task)
    stopTask();

  act("You put $p back on the anvil to resize it.", false, this, obj, 0,
    TO_CHAR);
  act("$n puts $p back on the anvil.", true, this, obj, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_FORGE, 8);

  // The race rides in status so the finish knows what shape to cut to.
  start_task(this, obj, nullptr, TASK_RESIZE, "",
    max(1, static_cast<int>(obj->getMaxStructPoints())), in_room,
    static_cast<ubyte>(race), 0, 0);
}

void tailorFinish(TBeing* ch, TObj* obj, race_t race) {
  TemplateSlot slot = getWearableSlot(obj);
  int wanted = getSlotVolumeForRace(slot, race);
  int had = obj->getVolume();

  if (wanted <= 0) {
    act("You cannot picture $p on a body that shape.", false, ch, obj, 0,
      TO_CHAR);
    return;
  }

  obj->setVolume(wanted);
  obj->setWeight(weightForVolume(wanted, obj->getMaterial()));

  const char* sizeName = raceSizeName(race);
  if (sizeName)
    ch->sendTo(format("You cut it down to %s size.\n\r") % sizeName);

  // Cloth cut away keeps what the piece carried, the same as metal does. It
  // has no crucible of its own -- a mage distills it for essence, or
  // transmutes it into something a smith can melt.
  if (!makeOffcut(ch, obj, had - wanted)) {
    act("You finish $p, and sweep the clippings away.", false, ch, obj, 0,
      TO_CHAR);
    return;
  }

  act("You finish $p, and fold the clippings aside.", false, ch, obj, 0,
    TO_CHAR);
  act("$n finishes work on $p.", true, ch, obj, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_TAILOR, obj);
}

void TBeing::doTailor(const char* argument) {
  sstring args(argument);
  sstring itemName = args.word(0);
  sstring raceName = args.word(1);

  if (!doesKnowSkill(SKILL_TAILOR)) {
    sendTo("You know nothing about cutting cloth to fit.\n\r");
    return;
  }

  if (itemName.empty() || raceName.empty()) {
    sendTo("Tailor what, for whom?\n\r");
    sendTo("Syntax: tailor <item> <race>\n\r");
    return;
  }

  TThing* found = searchLinkedListVis(this, itemName.c_str(), stuff);
  TObj* obj = dynamic_cast<TObj*>(found);
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(found);

  if (!obj || !clothing) {
    sendTo("You need to be carrying that to tailor it.\n\r");
    return;
  }

  // The other half of resize: cloth work, where Forge does metal. The split is
  // by material rather than by item type, so a leather cap and a steel one go
  // to different craftsmen even in the same slot.
  if (isMetalMaterial(obj->getMaterial())) {
    act("$p is metal. That is work for a forge.", false, this, obj, 0, TO_CHAR);
    return;
  }

  TemplateSlot slot = getWearableSlot(obj);
  if (slot == TemplateSlot::COUNT) {
    act("You cannot make sense of how $p is meant to be worn.", false, this,
      obj, 0, TO_CHAR);
    return;
  }

  race_t race = getRaceFromName(raceName);
  if (race == RACE_NORACE) {
    sendTo(format("You've never seen a %s to fit cloth to.\n\r") % raceName);
    return;
  }

  int wanted = getSlotVolumeForRace(slot, race);
  if (wanted <= 0) {
    sendTo(format("You have no pattern that would fit a %s.\n\r") % raceName);
    return;
  }

  if (wanted == obj->getVolume()) {
    act("$p is already that size.", false, this, obj, 0, TO_CHAR);
    return;
  }

  // Letting a piece out needs cloth to let it out with, the same way growing
  // a piece of armor needs a bar. Quality of the bolt does not enter it.
  if (wanted > obj->getVolume()) {
    int needUnits = max(1, static_cast<int>(
      weightForVolume(wanted - obj->getVolume(), obj->getMaterial()) * 10.0f));

    TCommodity* bolt = findCommodity(this, obj->getMaterial());
    if (!bolt || bolt->numUnits() < needUnits) {
      sendTo(format("Letting $p out that far needs %d units of %s, and you "
                    "have %d.\n\r") %
             needUnits % material_nums[obj->getMaterial()].mat_name %
             (bolt ? bolt->numUnits() : 0));
      return;
    }

    consumeCommodity(this, obj->getMaterial(), needUnits);
  }

  if (task)
    stopTask();

  act("You lay $p out and begin cutting it to fit.", false, this, obj, 0,
    TO_CHAR);
  act("$n lays $p out and begins cutting.", true, this, obj, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_TAILOR, 8);

  start_task(this, obj, nullptr, TASK_TAILOR, "",
    max(1, static_cast<int>(obj->getMaxStructPoints())), in_room,
    static_cast<ubyte>(race), 0, 0);
}

bool isSoftMaterial(unsigned short material) {
  return findHideMaterial(material) != nullptr;
}

int getHideDifficulty(unsigned short material) {
  const HideMaterial* hide = findHideMaterial(material);
  return hide ? hide->difficultyMod : 0;
}

int getHideStructure(unsigned short material) {
  const HideMaterial* hide = findHideMaterial(material);
  if (hide)
    return hide->structureMod;

  return max(1, static_cast<int>(material_nums[material].hardness));
}

int getFibreRollMod(const TBeing* ch, unsigned short material) {
  // The soft table's rarity is what resists: common cloth reads -10, silk and
  // laminate +40. Nothing buys it back the way advanced blacksmithing does for
  // metal, so fine fibre stays hard for everyone.
  return -getHideDifficulty(material);
}

int getSkeinStructure(unsigned short material, int units) {
  return max(1, getHideStructure(material) * max(0, units) / 100);
}

void weaveFinish(TBeing* ch, TObj* obj, int hits, int misses) {
  TSkein* skein = dynamic_cast<TSkein*>(read_object(kSkeinVnum, VIRTUAL));
  if (!skein) {
    act("$p comes apart in your hands and is simply gone.", false, ch, obj, 0,
      TO_CHAR);
    return;
  }

  skein->swapToStrung();

  int quality = getSmeltQuality(hits, misses);
  int units = getSmeltUnits(ch, obj->getWeight());

  skein->setSkeinQuality(quality);
  skein->setSkeinUnits(units);
  skein->setMaterial(obj->getMaterial());
  skein->setWeight(units / 10.0);
  skein->setVolume(volumeForWeight(units / 10.0f, obj->getMaterial()));
  skein->setMaxStructPoints(getSkeinStructure(obj->getMaterial(), units));
  skein->setStructPoints(skein->getMaxStructPoints());

  sstring fibre = material_nums[obj->getMaterial()].mat_name;
  skein->name = format("skein thread %s") % fibre;
  skein->shortDescr = format("a skein of %s thread") % fibre;
  skein->setDescr(format("A skein of %s thread lies here.") % fibre);

  // Same rule as the crucible: the stats come across, the armor value does
  // not, and a paired piece carried double what a single one did.
  bool paired = obj->isObjStat(ITEM_PAIRED);
  for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
    skein->affected[i] = obj->affected[i];

    if (skein->affected[i].location == APPLY_ARMOR) {
      skein->affected[i].location = APPLY_NONE;
      skein->affected[i].modifier = 0;
      continue;
    }

    if (paired)
      skein->affected[i].modifier /= 2;
  }

  --(*obj);
  delete obj;

  *ch += *skein;

  act("You wind $p off the last of it.", false, ch, skein, 0, TO_CHAR);
  act("$n winds $p off what $e was working.", true, ch, skein, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_WEAVE, skein);
}

void TBeing::doWeave(const char* argument) {
  char name_buf[256];

  if (!doesKnowSkill(SKILL_WEAVE)) {
    sendTo("You know nothing about working fibre.\n\r");
    return;
  }

  strcpy(name_buf, argument);

  if (!*name_buf) {
    sendTo("What is it you intend to weave down?\n\r");
    return;
  }

  TThing* found = searchLinkedListVis(this, name_buf, stuff);
  TObj* obj = dynamic_cast<TObj*>(found);
  if (!obj) {
    sendTo("You need to be carrying that to work it.\n\r");
    return;
  }

  // Material decides, the same way Smelt decides on metal. Anything made of
  // fibre comes apart into thread, whatever kind of thing it is.
  if (!isSoftMaterial(obj->getMaterial())) {
    act("$p is not made of anything you could draw a thread from.", false, this,
      obj, 0, TO_CHAR);
    return;
  }

  itemTypeT type = obj->itemType();
  if (type == ITEM_SKEIN || type == ITEM_RAW_MATERIAL) {
    act("$p is already so much thread.", false, this, obj, 0, TO_CHAR);
    return;
  }

  if (task)
    stopTask();

  act("You begin picking $p apart.", false, this, obj, 0, TO_CHAR);
  act("$n begins picking $p apart.", true, this, obj, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_WEAVE, 8);

  // Structure is the clock, and as with smelting nothing damages the piece on
  // the way: it is coming apart regardless, and what a miss costs is the grade
  // of the skein that comes out.
  start_task(this, obj, nullptr, TASK_WEAVE, "",
    max(1, static_cast<int>(obj->getMaxStructPoints())), in_room, 0, 0, 0);
}

bool isStatApply(int apply) {
  switch (apply) {
    case APPLY_STR:
    case APPLY_CON:
    case APPLY_BRA:
    case APPLY_DEX:
    case APPLY_AGI:
    case APPLY_SPE:
    case APPLY_INT:
    case APPLY_WIS:
    case APPLY_FOC:
    case APPLY_PER:
    case APPLY_CHA:
    case APPLY_KAR:
      return true;
    default:
      return false;
  }
}

bool isPoolApply(int apply) {
  return apply == APPLY_HIT || apply == APPLY_MANA || apply == APPLY_MOVE;
}

int getStatRank(const TObj* obj, int apply) {
  if (!obj || !isStatApply(apply))
    return 0;

  int rank = 0;

  for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
    if (!isStatApply(obj->affected[i].location) || !obj->affected[i].modifier)
      continue;

    rank++;

    // An apply already on the piece keeps the place it was given, so raising
    // it later does not push it down the order.
    if (obj->affected[i].location == apply)
      return rank;
  }

  // Not present: it would be the next one along.
  return rank + 1;
}

int getInfuseMax(const TObj* obj, int apply) {
  if (!isStatApply(apply))
    return 0;

  int rank = getStatRank(obj, apply);
  if (rank < 1 || rank > 3)
    return 0;

  return 6 - rank;
}

int getInfuseAmount(int apply, int quality) {
  // A point of mana is not a point of strength. Pools move in fives.
  return isPoolApply(apply) ? quality * 5 : quality;
}

void TBeing::doInfuse(const char* argument) {
  sstring args(argument);
  sstring itemName = args.word(0);
  sstring essenceName = args.word(1);

  if (!doesKnowSkill(SKILL_INFUSE)) {
    sendTo("You know nothing about writing virtue into a thing.\n\r");
    return;
  }

  if (itemName.empty() || essenceName.empty()) {
    sendTo("Infuse what into what?\n\r");
    sendTo("Syntax: infuse <item> <essence>\n\r");
    return;
  }

  TObj* obj =
    dynamic_cast<TObj*>(searchLinkedListVis(this, itemName.c_str(), stuff));
  if (!obj) {
    sendTo("You need to be carrying that to infuse it.\n\r");
    return;
  }

  TEssence* essence = dynamic_cast<TEssence*>(
    searchLinkedListVis(this, essenceName.c_str(), stuff));
  if (!essence) {
    sendTo("You have no such essence in hand.\n\r");
    return;
  }

  int apply = essence->getApplyType();
  int amount = getInfuseAmount(apply, essence->getQuality());
  const char* what = essenceApplyName(apply);

  // Find the apply if the piece already carries it, and the first empty slot
  // if it does not.
  int found = -1, empty = -1;
  for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
    if (obj->affected[i].location == apply && obj->affected[i].modifier) {
      found = i;
      break;
    }
    if (empty < 0 && (obj->affected[i].location == APPLY_NONE ||
                       !obj->affected[i].modifier))
      empty = i;
  }

  if (isStatApply(apply)) {
    int cap = getInfuseMax(obj, apply);

    if (!cap) {
      act("$p carries as many different virtues as it can hold.", false, this,
        obj, 0, TO_CHAR);
      return;
    }

    // The essence is the price and the position is the return: the whole
    // thing is spent either way, and a piece that already carries one virtue
    // gives back less for the next. Anything above the cap is simply lost.
    if (amount > cap) {
      sendTo(format("$p will take only +%d of %s from that; the rest is "
                    "lost.\n\r") %
             cap % what);
      amount = cap;
    }
  }

  if (found >= 0) {
    // Raising only. An essence that would write what the piece already has,
    // or less, is refused rather than spent.
    if (amount <= obj->affected[found].modifier) {
      sendTo(format("$p already carries +%d of %s.\n\r") %
             obj->affected[found].modifier % what);
      return;
    }
  } else if (empty < 0) {
    act("$p has no room left for anything more.", false, this, obj, 0, TO_CHAR);
    return;
  }

  if (task)
    stopTask();

  act("You begin working $p into $P.", false, this, essence, obj, TO_CHAR);
  act("$n begins working something into $P.", true, this, essence, obj,
    TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_INFUSE, 8);

  // The essence's name rides along: the finish needs it, and the essence is
  // reset rather than destroyed, whatever the size of what it wrote.
  start_task(this, obj, nullptr, TASK_INFUSE, essenceName.c_str(),
    max(1, static_cast<int>(obj->getMaxStructPoints())), in_room, 0, 0, 0);
}

void infuseFinish(TBeing* ch, TObj* obj, const char* essenceName) {
  TEssence* essence =
    dynamic_cast<TEssence*>(searchLinkedListVis(ch, essenceName, ch->stuff));
  if (!essence) {
    ch->sendTo("Your essence is gone, and the work comes to nothing.\n\r");
    return;
  }

  int apply = essence->getApplyType();
  int amount = getInfuseAmount(apply, essence->getQuality());
  const char* what = essenceApplyName(apply);

  // Diminishing return by position: the first virtue on a piece takes the
  // full write, the second one less, the third less again. What the essence
  // held above that is spent all the same.
  if (isStatApply(apply)) {
    int cap = getInfuseMax(obj, apply);
    if (!cap) {
      act("$p carries as many different virtues as it can hold.", false, ch,
        obj, 0, TO_CHAR);
      return;
    }
    amount = min(amount, cap);
  }

  int slot = -1;
  for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
    if (obj->affected[i].location == apply && obj->affected[i].modifier) {
      slot = i;
      break;
    }
  }

  if (slot < 0) {
    for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
      if (obj->affected[i].location == APPLY_NONE || !obj->affected[i].modifier) {
        slot = i;
        break;
      }
    }
  }

  if (slot < 0) {
    act("$p has no room left for anything more.", false, ch, obj, 0, TO_CHAR);
    return;
  }

  obj->affected[slot].location = static_cast<applyTypeT>(apply);
  obj->affected[slot].modifier = amount;

  // The whole essence goes, however little of it was written. Grow one to the
  // size you need and never overshoot.
  essence->setQuality(1);
  essence->setCharges(0);
  essence->swapToStrung();
  essence->shortDescr = format("an essence of %s (quality 1)") % what;

  act("You work the last of it into $p.", false, ch, obj, 0, TO_CHAR);
  ch->sendTo(format("It carries +%d of %s now.\n\r") % amount % what);
  act("$n finishes working something into $p.", true, ch, obj, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_INFUSE, obj);
}

MaterialFamily getMaterialFamily(unsigned short material) {
  if (findMetalMaterial(material))
    return FAM_METAL;
  if (findCrystalMaterial(material))
    return FAM_CRYSTAL;
  if (findRockMaterial(material))
    return FAM_ROCK;
  if (findDeadMaterial(material))
    return FAM_DEAD;
  if (findGenericMaterial(material))
    return FAM_GENERIC;
  if (findOrganicMaterial(material))
    return FAM_ORGANIC;
  if (findHideMaterial(material))
    return FAM_HIDE;
  if (findWoodMaterial(material))
    return FAM_WOOD;
  if (findMagicalMaterial(material))
    return FAM_MAGICAL;
  if (findSpiritualMaterial(material))
    return FAM_SPIRITUAL;

  return FAM_NONE;
}

int getMaterialRarity(unsigned short material) {
  if (const MetalMaterial* m = findMetalMaterial(material))
    return m->difficultyMod;
  if (const CrystalMaterial* m = findCrystalMaterial(material))
    return m->difficultyMod;
  if (const RockMaterial* m = findRockMaterial(material))
    return m->difficultyMod;
  if (const DeadMaterial* m = findDeadMaterial(material))
    return m->difficultyMod;
  if (const GenericMaterial* m = findGenericMaterial(material))
    return m->difficultyMod;
  if (const OrganicMaterial* m = findOrganicMaterial(material))
    return m->difficultyMod;
  if (const HideMaterial* m = findHideMaterial(material))
    return m->difficultyMod;
  if (const WoodMaterial* m = findWoodMaterial(material))
    return m->difficultyMod;
  if (const MagicalMaterial* m = findMagicalMaterial(material))
    return m->difficultyMod;
  if (const SpiritualMaterial* m = findSpiritualMaterial(material))
    return m->difficultyMod;

  return 0;
}

int getMaterialLevelReq(unsigned short material) {
  if (const MetalMaterial* m = findMetalMaterial(material))
    return m->levelMod;
  if (const CrystalMaterial* m = findCrystalMaterial(material))
    return m->levelMod;
  if (const RockMaterial* m = findRockMaterial(material))
    return m->levelMod;
  if (const DeadMaterial* m = findDeadMaterial(material))
    return m->levelMod;
  if (const GenericMaterial* m = findGenericMaterial(material))
    return m->levelMod;
  if (const OrganicMaterial* m = findOrganicMaterial(material))
    return m->levelMod;
  if (const HideMaterial* m = findHideMaterial(material))
    return m->levelMod;
  if (const WoodMaterial* m = findWoodMaterial(material))
    return m->levelMod;
  if (const MagicalMaterial* m = findMagicalMaterial(material))
    return m->levelMod;
  if (const SpiritualMaterial* m = findSpiritualMaterial(material))
    return m->levelMod;

  return 1;
}

// Whittle passes gainTaskExp the recipe's required knowledge and a multiplier
// of the piece's weight plus its difficulty grade. The same two quantities
// exist here already: levelMod is what the metal asks of a smith, and rarity
// is the grade. Nothing is invented, and nothing here depends on who is
// swinging the hammer -- advLearning inside gainTaskExp does that part.
void augmentTaskExp(TBeing* ch, spellNumT skill, const TObj* obj) {
  if (!ch || !obj)
    return;

  unsigned short material = obj->getMaterial();
  int baseLevel = max(1, getMaterialLevelReq(material));
  // getWeight() is a float and rarity an int, so the sum has to be widened
  // before max sees it against a double literal.
  double multiplier = max(
    1.0, static_cast<double>(obj->getWeight()) + getMaterialRarity(material));

  ch->gainTaskExp(skill, baseLevel, multiplier, false);
}

int getTransmuteRollMod(unsigned short from, unsigned short to) {
  MaterialFamily a = getMaterialFamily(from);
  MaterialFamily b = getMaterialFamily(to);

  if (a == FAM_NONE || b == FAM_NONE)
    return -100;

  int distance = abs(static_cast<int>(a) - static_cast<int>(b));
  int cost = distance * 10 + getMaterialRarity(to);

  // Inside one family the starting material is half the work already done:
  // mithril into starmetal is a short reach, while cloth into starmetal is
  // the length of the table.
  if (!distance)
    cost -= getMaterialRarity(from) / 2;

  return -cost;
}

int getTransmuteLength(unsigned short from, unsigned short to, float weight) {
  MaterialFamily a = getMaterialFamily(from);
  MaterialFamily b = getMaterialFamily(to);

  int distance = (a == FAM_NONE || b == FAM_NONE)
                   ? 10
                   : abs(static_cast<int>(a) - static_cast<int>(b));

  return max(1, distance + getMaterialRarity(to) + static_cast<int>(weight));
}

void transmuteFinish(TBeing* ch, TObj* obj, unsigned short material, int hits,
  int misses) {
  int total = hits + misses;
  int landed = total ? (hits * 100 / total) : 0;

  // The whole working is judged at once. Individual pulses do not change
  // anything; they are only the tally that decides whether the change takes.
  if (landed < kTransmuteSuccessRatio) {
    act("The working comes apart, and $p is what it always was.", false, ch,
      obj, 0, TO_CHAR);
    act("Something around $n's hands collapses quietly.", true, ch, obj, 0,
      TO_ROOM);
    return;
  }

  obj->setMaterial(material);

  // Volume is what a thing is; weight is what that volume of this stuff comes
  // to. Change the material and the weight follows, which is why a mithril
  // breastplate is lighter than the steel one it used to be.
  if (obj->getVolume() > 0)
    obj->setWeight(weightForVolume(obj->getVolume(), material));

  act("$p shivers, and is something else now.", false, ch, obj, 0, TO_CHAR);
  ch->sendTo(format("It is %s.\n\r") % material_nums[material].mat_name);
  act("$p shivers in $n's hands and changes.", true, ch, obj, 0, TO_ROOM);

  augmentTaskExp(ch, SKILL_TRANSMUTE, obj);
}

void TBeing::doTransmute(const char* argument) {
  sstring args(argument);
  sstring itemName = args.word(0);
  sstring matName = args.word(1);

  if (!doesKnowSkill(SKILL_TRANSMUTE)) {
    sendTo("You know nothing about changing what a thing is made of.\n\r");
    return;
  }

  if (itemName.empty() || matName.empty()) {
    sendTo("Transmute what into what?\n\r");
    sendTo("Syntax: transmute <item> <material>\n\r");
    return;
  }

  TObj* obj =
    dynamic_cast<TObj*>(searchLinkedListVis(this, itemName.c_str(), stuff));
  if (!obj) {
    sendTo("You need to be carrying that to transmute it.\n\r");
    return;
  }

  int material = getMaterialFromName(matName);
  if (material < 0) {
    sendTo(format("You've never heard of %s.\n\r") % matName);
    return;
  }

  if (getMaterialFamily(material) == FAM_NONE) {
    sendTo(format("%s is not a thing you could make something out of.\n\r") %
           matName);
    return;
  }

  if (static_cast<unsigned short>(material) == obj->getMaterial()) {
    act("$p is already made of that.", false, this, obj, 0, TO_CHAR);
    return;
  }

  if (getMaterialFamily(obj->getMaterial()) == FAM_NONE) {
    act("You cannot make out what $p is made of at all.", false, this, obj, 0,
      TO_CHAR);
    return;
  }

  // An opal is the price of the working, and it is paid whether or not the
  // change takes. Its carats are what makes a hard reach possible at all:
  // every one of them buys ten off the difficulty.
  TOpal* opal = nullptr;
  for (StuffIter it = stuff.begin(); it != stuff.end(); ++it) {
    TOpal* candidate = dynamic_cast<TOpal*>(*it);
    if (candidate) {
      opal = candidate;
      break;
    }
  }

  if (!opal) {
    sendTo("You have no opal to spend on the working.\n\r");
    return;
  }

  int carats = max(0, opal->psGetCarats());
  int mod = getTransmuteRollMod(obj->getMaterial(),
              static_cast<unsigned short>(material)) +
            carats * 10;

  int pulses = getTransmuteLength(obj->getMaterial(),
    static_cast<unsigned short>(material), obj->getWeight());

  if (task)
    stopTask();

  act("You crush $p into the working, and it begins.", false, this, opal, 0,
    TO_CHAR);
  act("$n crushes $p, and the air goes strange.", true, this, opal, 0, TO_ROOM);

  --(*opal);
  delete opal;

  act("You begin working the substance of $p.", false, this, obj, 0, TO_CHAR);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_TRANSMUTE, 8);

  // The target material rides in status and the roll modifier in orig_arg --
  // the opal is gone by the first pulse, so what it bought has to be carried
  // rather than recomputed. The tallies live in flags.
  start_task(this, obj, nullptr, TASK_TRANSMUTE,
    (format("%d") % mod).str().c_str(), pulses, in_room,
    static_cast<ubyte>(material), 0, 0);
}

int getForgeWeaponMax(const TBeing* ch) {
  // Level 70 mobs load level 60 gear, and a player stands a step below that:
  // six sevenths. The level that goes into it stops counting at 35, so the
  // anvil alone tops out at 30 no matter who is standing at it.
  return max(1, 6 * min(35, static_cast<int>(ch->GetMaxLevel())) / 7);
}

int getHoneMax(const TBeing* ch) {
  return static_cast<int>(ch->GetMaxLevel()) +
         ch->getSkillValue(SKILL_FORGE) / 20;
}

bool canForgeWeapon(const TBeing* ch, int maxSharp) {
  // Sharpness is the difficulty of the kind: a pickaxe at 50 asks nothing, a
  // stiletto at 95 asks a great deal.
  return ch->getSkillValue(SKILL_FORGE) > (maxSharp - 50);
}

void TBeing::doForgeWeapon(const char* argument) {
  sstring args(argument);
  sstring weaponName = args.word(1);
  sstring ingotName = args.word(2);

  if (weaponName.empty() || ingotName.empty()) {
    sendTo("Forge what weapon, out of what?\n\r");
    sendTo("Syntax: forge weapon <kind> <ingot>\n\r");
    return;
  }

  ForgeWeaponSpec spec;
  if (!findWeaponSpecByName(weaponName.c_str(), &spec)) {
    sendTo(format("You've never heard of a %s.\n\r") % weaponName);
    return;
  }

  if (!canForgeWeapon(this, spec.maxSharp)) {
    sendTo(format("A %s is finer work than you can manage.\n\r") % spec.name);
    return;
  }

  TIngot* ingot =
    dynamic_cast<TIngot*>(searchLinkedListVis(this, ingotName.c_str(), stuff));
  if (!ingot) {
    sendTo("You need an ingot in hand to forge from.\n\r");
    return;
  }

  float needWeight = weightForVolume(spec.volume, ingot->getMaterial());
  int needUnits = max(1, static_cast<int>(needWeight * 10.0f));

  if (ingot->getIngotUnits() < needUnits) {
    sendTo(format("A %s needs %d units of metal, and $p holds %d.\n\r") %
           spec.name % needUnits % ingot->getIngotUnits());
    return;
  }

  TObj* weapon = read_object(kWeaponVnum, VIRTUAL);
  TGenWeapon* blade = dynamic_cast<TGenWeapon*>(weapon);
  if (!blade) {
    if (weapon)
      delete weapon;
    sendTo("You cannot picture how that would go together.\n\r");
    return;
  }

  weapon->swapToStrung();
  weapon->setMaterial(ingot->getMaterial());
  weapon->setVolume(spec.volume);
  weapon->setWeight(needWeight);

  if (spec.twoHanded)
    weapon->addObjStat(ITEM_PAIRED);

  nameCraftedWeapon(weapon, spec.name, ingot->getMaterial(), nullptr);

  // The kind decides how it cuts and how keen it can be; the smith decides
  // how hard it hits.
  blade->setWeaponType(static_cast<weaponT>(spec.type1), 0);
  blade->setWeaponFreq(spec.freq1, 0);
  if (spec.type2 != WEAPON_TYPE_NONE) {
    blade->setWeaponType(static_cast<weaponT>(spec.type2), 1);
    blade->setWeaponFreq(spec.freq2, 1);
  }

  blade->setMaxSharp(spec.maxSharp);
  blade->setCurSharp(spec.maxSharp);

  int damage = getForgeWeaponMax(this);
  blade->setWeapDamLvl(damage);
  blade->setWeapDamDev(max(0, 10 - damage / 6));

  int maxStruct = max(1, static_cast<int>(damage * 1.5 + 10.0));
  weapon->setMaxStructPoints(static_cast<short>(maxStruct));
  weapon->setStructPoints(static_cast<short>(maxStruct));

  for (int i = 0; i < MAX_OBJ_AFFECT; i++)
    weapon->affected[i] = ingot->affected[i];

  int quality = ingot->getIngotQuality();

  int left = ingot->getIngotUnits() - needUnits;
  if (left <= 0) {
    --(*ingot);
    delete ingot;
  } else {
    ingot->setIngotUnits(left);
    ingot->setWeight(left / 10.0);
    ingot->setVolume(volumeForWeight(left / 10.0f, ingot->getMaterial()));
    ingot->setMaxStructPoints(getIngotStructure(ingot->getMaterial(), left));
    ingot->setStructPoints(ingot->getMaxStructPoints());
  }

  *this += *weapon;

  if (task)
    stopTask();

  act("You lay $p out on the anvil and begin.", false, this, weapon, 0,
    TO_CHAR);
  act("$n lays $p out on the anvil.", true, this, weapon, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_FORGE, 8);

  start_task(this, weapon, nullptr, TASK_FORGE, ingotName.c_str(),
    max(1, static_cast<int>(weapon->getMaxStructPoints())), in_room, quality, 0,
    0);
}

void TBeing::doForgeHone(const char* argument) {
  sstring args(argument);
  sstring weaponName = args.word(1);

  if (weaponName.empty()) {
    sendTo("Hone what?\n\r");
    sendTo("Syntax: forge hone <weapon>\n\r");
    return;
  }

  TGenWeapon* blade = dynamic_cast<TGenWeapon*>(
    searchLinkedListVis(this, weaponName.c_str(), stuff));
  if (!blade) {
    sendTo("You need to be carrying that weapon to hone it.\n\r");
    return;
  }

  int ceiling = getHoneMax(this);
  if (blade->getWeapDamLvl() >= ceiling) {
    act("$p is already sharper than you know how to make it.", false, this,
      blade, 0, TO_CHAR);
    return;
  }

  if (task)
    stopTask();

  act("You set to honing $p.", false, this, blade, 0, TO_CHAR);
  act("$n sets to honing $p.", true, this, blade, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_FORGE, 8);

  // Ten landed pulses to the point. No fixed end: honing runs until the
  // ceiling, until the smith is spent, or until something stops them.
  start_task(this, blade, nullptr, TASK_HONE, "", 10, in_room, 0, 0, 0);
}

// A piece knows its slot and its volume, and the two together say what body
// it was cut for. Refitting to another slot has to keep that body: a gnome's
// bracer becomes a gnome's greave, not a human's.
race_t getRaceForVolume(TemplateSlot slot, int volume) {
  static const race_t candidates[] = {RACE_HOBBIT, RACE_GNOME, RACE_DWARF,
    RACE_ELVEN, RACE_HUMAN, RACE_OGRE};

  race_t best = RACE_HUMAN;
  int bestGap = -1;

  for (race_t race : candidates) {
    int v = getSlotVolumeForRace(slot, race);
    if (v <= 0)
      continue;

    int gap = abs(v - volume);
    if (bestGap < 0 || gap < bestGap) {
      bestGap = gap;
      best = race;
    }
  }

  return best;
}

double getSlotArmorShare(TemplateSlot slot) {
  // Mirrors TBaseClothing::armorPercs(). Kept here as its own table because
  // this system owns what a slot is worth when moving a piece between two of
  // them, and armorPercs answers a different question -- what an item already
  // on a slot is worth.
  switch (slot) {
    case TemplateSlot::Shield:
      return 0.25;
    case TemplateSlot::Body:
      return 0.15;
    case TemplateSlot::Waist:
      return 0.08;
    case TemplateSlot::Head:
    case TemplateSlot::Back:
      return 0.07;
    case TemplateSlot::Leg:
      return 0.05;
    case TemplateSlot::Neck:
    case TemplateSlot::Arm:
      return 0.04;
    case TemplateSlot::Hand:
      return 0.03;
    case TemplateSlot::Wrist:
    case TemplateSlot::Foot:
      return 0.02;
    case TemplateSlot::Finger:
      return 0.01;
    default:
      return 0.01;
  }
}

void refitFinish(TBeing* ch, TObj* obj, TemplateSlot slot) {
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(obj);
  if (!clothing)
    return;

  TemplateSlot from = getWearableSlot(obj);
  if (from == TemplateSlot::COUNT || from == slot)
    return;

  Tier tier = getWearableTier(clothing);
  double level = clothing->armorLevel(ARMOR_LEV_REAL);

  // The same armor, moved. A slot that carries more of a suit's protection
  // needs a lower level to offer the same absolute defence, and a smaller slot
  // a higher one -- so the piece keeps what it was worth rather than gaining
  // or losing by the move alone.
  double share = getSlotArmorShare(slot);
  if (share <= 0.0)
    return;

  level = level * getSlotArmorShare(from) / share;

  // Except upward, where the ceiling bites. Packing a breastplate into a
  // bracer loses whatever will not fit; it does not concentrate.
  double ceiling = getTierSkillMax(tier);
  if (ceiling > 0.0)
    level = min(level, ceiling);

  // The piece has to become the size of the thing it is now. Whatever is left
  // over comes off as an offcut, the same as a resize -- and what it carried
  // goes with it.
  race_t race = getRaceForVolume(from, obj->getVolume());
  int wanted = getSlotVolumeForRace(slot, race);
  int had = obj->getVolume();

  TObj* fresh = convertWearableSlot(ch, obj, slot);
  if (!fresh) {
    act("$p will not take the shape you want of it.", false, ch, obj, 0,
      TO_CHAR);
    return;
  }

  TBaseClothing* worked = dynamic_cast<TBaseClothing*>(fresh);
  if (!worked)
    return;

  if (wanted > 0) {
    fresh->setVolume(wanted);
    fresh->setWeight(weightForVolume(wanted, fresh->getMaterial()));
    makeOffcut(ch, fresh, had - wanted);
  }

  worked->setDefArmorLevel(static_cast<float>(level));

  act("You work $p onto a different part of the body entirely.", false, ch,
    fresh, 0, TO_CHAR);
  act("$n finishes reworking $p.", true, ch, fresh, 0, TO_ROOM);

  augmentTaskExp(ch, getMaterialFamily(fresh->getMaterial()) == FAM_METAL ? SKILL_FORGE
                                                            : SKILL_SEW, fresh);
}

// forge refit and sew refit are one function: the material decides which
// craftsman may do it and what the difference is paid in, exactly as with
// resizing.
void TBeing::doRefit(const char* argument, bool metal) {
  sstring args(argument);
  sstring itemName = args.word(1);
  sstring slotName = args.word(2);

  spellNumT skill = metal ? SKILL_FORGE : SKILL_SEW;

  if (itemName.empty() || slotName.empty()) {
    sendTo("Refit what, onto what?\n\r");
    sendTo(format("Syntax: %s refit <item> <slot>\n\r") %
           (metal ? "forge" : "sew"));
    return;
  }

  TThing* found = searchLinkedListVis(this, itemName.c_str(), stuff);
  TObj* obj = dynamic_cast<TObj*>(found);
  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(found);

  if (!obj || !clothing) {
    sendTo("You need to be carrying that to refit it.\n\r");
    return;
  }

  if (isMetalMaterial(obj->getMaterial()) != metal) {
    if (metal)
      act("$p is not metal. That is work for a tailor.", false, this, obj, 0,
        TO_CHAR);
    else
      act("$p is metal. That is work for a forge.", false, this, obj, 0,
        TO_CHAR);
    return;
  }

  TemplateSlot from = getWearableSlot(obj);
  TemplateSlot slot = getTemplateSlotFromName(slotName);

  if (from == TemplateSlot::COUNT) {
    act("You cannot make sense of how $p is meant to be worn.", false, this,
      obj, 0, TO_CHAR);
    return;
  }

  if (slot == TemplateSlot::COUNT) {
    sendTo(format("There is no %s to fit anything to.\n\r") % slotName);
    return;
  }

  if (slot == from) {
    act("$p is already worn there.", false, this, obj, 0, TO_CHAR);
    return;
  }

  // The body it was cut for stays the body it is cut for.
  race_t race = getRaceForVolume(from, obj->getVolume());
  int wanted = getSlotVolumeForRace(slot, race);

  if (wanted <= 0) {
    sendTo("You have no pattern for that.\n\r");
    return;
  }

  // Growing into a larger slot needs material for the difference, the same as
  // letting a piece out does. Shrinking leaves an offcut instead.
  if (wanted > obj->getVolume()) {
    int needUnits = max(1, static_cast<int>(
      weightForVolume(wanted - obj->getVolume(), obj->getMaterial()) * 10.0f));

    if (metal) {
      TIngot* bar = nullptr;
      for (StuffIter it = stuff.begin(); it != stuff.end(); ++it) {
        TIngot* candidate = dynamic_cast<TIngot*>(*it);
        if (candidate && candidate->getMaterial() == obj->getMaterial() &&
            candidate->getIngotUnits() >= needUnits) {
          bar = candidate;
          break;
        }
      }

      if (!bar) {
        sendTo(format("That would need %d units of %s, and you have no bar "
                      "with that much in it.\n\r") %
               needUnits % material_nums[obj->getMaterial()].mat_name);
        return;
      }

      int left = bar->getIngotUnits() - needUnits;
      if (left <= 0) {
        --(*bar);
        delete bar;
      } else {
        bar->setIngotUnits(left);
        bar->setWeight(left / 10.0);
        bar->setVolume(volumeForWeight(left / 10.0f, bar->getMaterial()));
        bar->setMaxStructPoints(getIngotStructure(bar->getMaterial(), left));
        bar->setStructPoints(bar->getMaxStructPoints());
      }
    } else {
      TCommodity* bolt = findCommodity(this, obj->getMaterial());
      if (!bolt || bolt->numUnits() < needUnits) {
        sendTo(format("That would need %d units of %s, and you have %d.\n\r") %
               needUnits % material_nums[obj->getMaterial()].mat_name %
               (bolt ? bolt->numUnits() : 0));
        return;
      }

      consumeCommodity(this, obj->getMaterial(), needUnits);
    }
  }

  if (task)
    stopTask();

  act("You begin working $p onto a different part of the body.", false, this,
    obj, 0, TO_CHAR);
  act("$n begins reworking $p.", true, this, obj, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, skill, 8);

  start_task(this, obj, nullptr, metal ? TASK_REFIT : TASK_REFIT_CLOTH, "",
    max(1, static_cast<int>(obj->getMaxStructPoints())), in_room,
    static_cast<ubyte>(slot), 0, 0);
}

void TBeing::doStrip(const char* argument) {
  char name_buf[256];

  if (!doesKnowSkill(SKILL_STRIP)) {
    sendTo("You know nothing about stripping armor down.\n\r");
    return;
  }

  strcpy(name_buf, argument);

  if (!*name_buf) {
    sendTo("What is it you intend to strip?\n\r");
    return;
  }

  TThing* found = searchLinkedListVis(this, name_buf, stuff);
  TObj* obj = dynamic_cast<TObj*>(found);
  if (!obj) {
    sendTo("You need to be carrying that to strip it.\n\r");
    return;
  }

  // Keyed on the stored item type, not a dynamic_cast: a TArmorWand is
  // genuinely a TArmor, and converting one would drop its wand half.
  itemTypeT type = obj->itemType();
  if (type != ITEM_ARMOR && type != ITEM_WORN) {
    act("$p is not something you can cut down.", false, this, obj, 0, TO_CHAR);
    return;
  }

  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(obj);
  Tier tier = getWearableTier(clothing);
  if (getTierBelow(tier) == Tier_Max) {
    act("$p is already as plain as it gets.", false, this, obj, 0, TO_CHAR);
    return;
  }

  if (getWearableSlot(obj) == TemplateSlot::COUNT) {
    act("You cannot make sense of how $p is meant to be worn.", false, this,
      obj, 0, TO_CHAR);
    return;
  }

  if (task)
    stopTask();

  act("You start cutting away at $p.", false, this, obj, 0, TO_CHAR);
  act("$n starts cutting away at $p.", true, this, obj, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_STRIP, 8);

  // Structure is the clock. A heavier piece takes longer to cut down, and
  // damage taken along the way does not shorten the job.
  start_task(this, obj, nullptr, TASK_STRIP, "", obj->getMaxStructPoints(),
    in_room, 0, 0, 0);
}
void TBeing::doPlate(const char* argument) {
  char name_buf[256];

  if (!doesKnowSkill(SKILL_PLATE)) {
    sendTo("You know nothing about plating armor.\n\r");
    return;
  }

  strcpy(name_buf, argument);

  if (!*name_buf) {
    sendTo("What is it you intend to plate?\n\r");
    return;
  }

  TThing* found = searchLinkedListVis(this, name_buf, stuff);
  TObj* obj = dynamic_cast<TObj*>(found);
  if (!obj) {
    sendTo("You need to be carrying that to plate it.\n\r");
    return;
  }

  itemTypeT type = obj->itemType();
  if (type != ITEM_ARMOR && type != ITEM_WORN) {
    act("$p is not something you can build up.", false, this, obj, 0, TO_CHAR);
    return;
  }

  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(obj);
  Tier tier = getWearableTier(clothing);
  Tier target = getTierAbove(tier);
  if (target == Tier_Max) {
    act("$p is already as heavy as armor gets.", false, this, obj, 0, TO_CHAR);
    return;
  }

  // Material is the gate on the ladder: a silk shirt cannot become plate until
  // a mage transmutes it into something that will hold the shape.
  if (target > getMaxTierForMaterial(obj->getMaterial())) {
    act("$p is made of too soft a stuff to carry any more than it does.", false,
      this, obj, 0, TO_CHAR);
    return;
  }

  if (getWearableSlot(obj) == TemplateSlot::COUNT) {
    act("You cannot make sense of how $p is meant to be worn.", false, this,
      obj, 0, TO_CHAR);
    return;
  }

  if (task)
    stopTask();

  act("You start working $p over.", false, this, obj, 0, TO_CHAR);
  act("$n starts working $p over.", true, this, obj, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_PLATE, 8);

  start_task(this, obj, nullptr, TASK_PLATE, "", obj->getMaxStructPoints(),
    in_room, 0, 0, 0);
}
void TBeing::doBangle(const char* argument) {
  char name_buf[256];

  if (!doesKnowSkill(SKILL_BANGLE)) {
    sendTo("You know nothing about working metal into ornament.\n\r");
    return;
  }

  strcpy(name_buf, argument);

  if (!*name_buf) {
    sendTo("What is it you intend to bangle?\n\r");
    return;
  }

  TThing* found = searchLinkedListVis(this, name_buf, stuff);
  TObj* obj = dynamic_cast<TObj*>(found);
  if (!obj) {
    sendTo("You need to be carrying that to bangle it.\n\r");
    return;
  }

  itemTypeT type = obj->itemType();
  if (type == ITEM_JEWELRY) {
    act("$p is already an ornament.", false, this, obj, 0, TO_CHAR);
    return;
  }
  if (type != ITEM_ARMOR && type != ITEM_WORN) {
    act("$p is not something you can work into ornament.", false, this, obj, 0,
      TO_CHAR);
    return;
  }

  if (getWearableSlot(obj) == TemplateSlot::COUNT) {
    act("You cannot make sense of how $p is meant to be worn.", false, this,
      obj, 0, TO_CHAR);
    return;
  }

  if (task)
    stopTask();

  act("You begin working $p down.", false, this, obj, 0, TO_CHAR);
  act("$n begins working $p down.", true, this, obj, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_BANGLE, 8);

  start_task(this, obj, nullptr, TASK_BANGLE, "", obj->getMaxStructPoints(),
    in_room, 0, 0, 0);
}
void TBeing::doSew(const char* argument) {
  sstring args(argument);

  if (sstring(args.word(0)).lower() == "refit") {
    doRefit(argument, false);
    return;
  }

  sstring slotName = args.word(0);
  sstring raceName = args.word(1);
  sstring skeinName = args.word(2);
  sstring tierName = args.word(3);

  if (!doesKnowSkill(SKILL_SEW)) {
    sendTo("You know nothing about sewing.\n\r");
    return;
  }

  if (slotName.empty() || raceName.empty() || skeinName.empty() ||
      tierName.empty()) {
    sendTo("Sew what, for whom, out of what, and how heavy?\n\r");
    sendTo("Syntax: sew <slot> <race> <skein> <tier>\n\r");
    sendTo("Tiers: clothing light\n\r");
    return;
  }

  TemplateSlot slot = getTemplateSlotFromName(slotName);
  if (slot == TemplateSlot::COUNT || slot == TemplateSlot::Finger) {
    sendTo(format("You wouldn't know how to sew something for a %s.\n\r") %
           slotName);
    return;
  }

  race_t race = getRaceFromName(raceName);
  if (race == RACE_NORACE) {
    sendTo(format("You've never seen a %s to cut cloth for.\n\r") % raceName);
    return;
  }

  int volume = getSlotVolumeForRace(slot, race);
  if (volume <= 0) {
    sendTo(format("You have no pattern that would fit a %s.\n\r") % raceName);
    return;
  }

  TSkein* skein =
    dynamic_cast<TSkein*>(searchLinkedListVis(this, skeinName.c_str(), stuff));
  if (!skein) {
    sendTo("You need a skein in hand to sew from.\n\r");
    return;
  }

  Tier tier = getTierFromName(tierName);
  if (tier != Tier_Clothing && tier != Tier_Light) {
    sendTo("Sewn work comes in clothing and light.\n\r");
    return;
  }

  // Hardness caps this the same way it caps the forge: leather reaches light,
  // silk never will, however fine it is.
  if (tier > getMaxTierForMaterial(skein->getMaterial())) {
    act("$p is too soft a fibre to carry that much.", false, this, skein, 0,
      TO_CHAR);
    return;
  }

  float needWeight = weightForVolume(volume, skein->getMaterial());
  int needUnits = max(1, static_cast<int>(needWeight * 10.0f));

  if (skein->getSkeinUnits() < needUnits) {
    sendTo(format("That piece needs %d units of thread, and $p holds %d.\n\r") %
           needUnits % skein->getSkeinUnits());
    return;
  }

  // Light work is armor and takes the armor template; clothing takes its own.
  itemTypeT type = (tier == Tier_Light) ? ITEM_ARMOR : ITEM_WORN;

  TObj* piece = makeBlankWearable(type, slot);
  if (!piece) {
    sendTo("You cannot picture how that would go together.\n\r");
    return;
  }

  piece->setMaterial(skein->getMaterial());
  piece->setVolume(volume);
  piece->setWeight(needWeight);
  piece->addObjStat(getTierFlags(tier));

  nameCraftedWearable(piece, tier, slot, race, skein->getMaterial(), nullptr);

  for (int i = 0; i < MAX_OBJ_AFFECT; i++)
    piece->affected[i] = skein->affected[i];

  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(piece);
  if (clothing)
    clothing->setDefArmorLevel(static_cast<float>(getForgeLevelMax(this, tier)));

  int quality = skein->getSkeinQuality();

  int left = skein->getSkeinUnits() - needUnits;
  if (left <= 0) {
    --(*skein);
    delete skein;
  } else {
    skein->setSkeinUnits(left);
    skein->setWeight(left / 10.0);
    skein->setVolume(volumeForWeight(left / 10.0f, skein->getMaterial()));
    skein->setMaxStructPoints(getSkeinStructure(skein->getMaterial(), left));
    skein->setStructPoints(skein->getMaxStructPoints());
  }

  *this += *piece;

  if (task)
    stopTask();

  act("You lay $p out and begin.", false, this, piece, 0, TO_CHAR);
  act("$n lays $p out and begins sewing.", true, this, piece, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_SEW, 8);

  // Same carriers as the forge: the skein's grade in status, the running
  // penalty in flags, and the skein's name so a miss can spoil what is left.
  start_task(this, piece, nullptr, TASK_SEW, skeinName.c_str(),
    max(1, static_cast<int>(piece->getMaxStructPoints())), in_room, quality, 0,
    0);
}

void TBeing::doForgePiece(const char* argument) {
  sstring args(argument);
  sstring slotName = args.word(0);
  sstring raceName = args.word(1);
  sstring ingotName = args.word(2);
  sstring tierName = args.word(3);

  if (slotName.empty() || raceName.empty() || ingotName.empty() ||
      tierName.empty()) {
    sendTo("Forge what, for whom, out of what, and how heavy?\n\r");
    sendTo("Syntax: forge <slot> <race> <ingot> <tier>\n\r");
    sendTo("        forge combine <ingot> <ingot>\n\r");
    sendTo("        forge resize <item> <race>\n\r");
    sendTo("        forge weapon <kind> <ingot>\n\r");
    sendTo("        forge hone <weapon>\n\r");
    sendTo("Tiers: light medium heavy\n\r");
    return;
  }

  TemplateSlot slot = getTemplateSlotFromName(slotName);
  if (slot == TemplateSlot::COUNT || slot == TemplateSlot::Finger) {
    sendTo(format("You wouldn't know how to forge armor for a %s.\n\r") %
           slotName);
    return;
  }

  race_t race = getRaceFromName(raceName);
  if (race == RACE_NORACE) {
    sendTo(format("You've never seen a %s to size armor for.\n\r") % raceName);
    return;
  }

  int volume = getSlotVolumeForRace(slot, race);
  if (volume <= 0) {
    sendTo(format("You have no pattern that would fit a %s.\n\r") % raceName);
    return;
  }

  TIngot* ingot =
    dynamic_cast<TIngot*>(searchLinkedListVis(this, ingotName.c_str(), stuff));
  if (!ingot) {
    sendTo("You need an ingot in hand to forge from.\n\r");
    return;
  }

  Tier tier = getTierFromName(tierName);
  if (tier == Tier_Max || tier == Tier_Clothing) {
    sendTo("Armor comes in light, medium and heavy.\n\r");
    return;
  }

  // Hardness caps the ladder here exactly as it does for Plate: soft metal
  // will not hold a heavy shape no matter how good the smith.
  if (tier > getMaxTierForMaterial(ingot->getMaterial())) {
    act("$p is too soft a metal to carry that much armor.", false, this, ingot,
      0, TO_CHAR);
    return;
  }

  // The bar has to hold enough metal for a piece this size. Volume, not
  // weight: a pound of mithril is a great deal more metal than a pound of
  // iron.
  float needWeight = weightForVolume(volume, ingot->getMaterial());
  int needUnits = max(1, static_cast<int>(needWeight * 10.0f));

  if (ingot->getIngotUnits() < needUnits) {
    sendTo(format("That piece needs %d units of metal, and $p holds %d.\n\r") %
           needUnits % ingot->getIngotUnits());
    return;
  }

  TObj* piece = makeBlankWearable(ITEM_ARMOR, slot);
  if (!piece) {
    sendTo("You cannot picture how that would go together.\n\r");
    return;
  }

  piece->setMaterial(ingot->getMaterial());
  piece->setVolume(volume);
  piece->setWeight(needWeight);
  piece->addObjStat(getTierFlags(tier));

  nameCraftedWearable(piece, tier, slot, race, ingot->getMaterial(), nullptr);

  // What the metal remembered comes across whole. The quality of the bar
  // decides how much of it survives the work, one point at a time, as the
  // smith misses.
  for (int i = 0; i < MAX_OBJ_AFFECT; i++)
    piece->affected[i] = ingot->affected[i];

  TBaseClothing* clothing = dynamic_cast<TBaseClothing*>(piece);
  if (clothing)
    clothing->setDefArmorLevel(static_cast<float>(getForgeLevelMax(this, tier)));

  // Read the grade before the bar is spent: it may not exist a moment from
  // now, and every miss in the work ahead is measured against it.
  int quality = ingot->getIngotQuality();

  // The metal is spent up front. A piece abandoned half-made does not give it
  // back, the same as a sewn piece.
  int left = ingot->getIngotUnits() - needUnits;
  if (left <= 0) {
    --(*ingot);
    delete ingot;
  } else {
    ingot->setIngotUnits(left);
    ingot->setWeight(left / 10.0);
    ingot->setVolume(volumeForWeight(left / 10.0f, ingot->getMaterial()));
    ingot->setMaxStructPoints(getIngotStructure(ingot->getMaterial(), left));
    ingot->setStructPoints(ingot->getMaxStructPoints());
  }

  *this += *piece;

  if (task)
    stopTask();

  act("You lay $p out on the anvil and begin.", false, this, piece, 0, TO_CHAR);
  act("$n lays $p out on the anvil.", true, this, piece, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_FORGE, 8);

  // The bar's name rides along so a missed pulse can find what is left of it:
  // botched work spoils metal that was still on the bench, not only the piece
  // on the anvil. The grade rides in status; the running penalty in flags.
  start_task(this, piece, nullptr, TASK_FORGE, ingotName.c_str(),
    max(1, static_cast<int>(piece->getMaxStructPoints())), in_room, quality, 0,
    0);
}

namespace {
  // Both bars have to be in hand, the same metal, and the donor blank: merging
  // two sets of affects has no rule, so the second bar may carry none.
  bool combineCheck(TBeing* ch, TIngot* into, TIngot* from) {
    if (!into || !from) {
      ch->sendTo("You need two ingots in hand to combine them.\n\r");
      return false;
    }

    if (into == from) {
      ch->sendTo("You cannot combine an ingot with itself.\n\r");
      return false;
    }

    if (into->getMaterial() != from->getMaterial()) {
      ch->sendTo("Those are different metals, and they will not take to each "
                 "other.\n\r");
      return false;
    }

    if (hasApplies(from)) {
      act("$p carries too much of what it used to be to melt into another "
          "bar.",
        false, ch, from, 0, TO_CHAR);
      return false;
    }

    return true;
  }
}  // namespace

void TBeing::doForge(const char* argument) {
  if (!doesKnowSkill(SKILL_FORGE)) {
    sendTo("You know nothing about forging.\n\r");
    return;
  }

  sstring args(argument);
  sstring first = args.word(0);

  if (first.lower() == "resize") {
    doForgeResize(argument);
    return;
  }

  if (first.lower() == "weapon") {
    doForgeWeapon(argument);
    return;
  }

  if (first.lower() == "refit") {
    doRefit(argument, true);
    return;
  }

  if (first.lower() == "hone") {
    doForgeHone(argument);
    return;
  }

  if (first.lower() != "combine") {
    doForgePiece(argument);
    return;
  }

  sstring intoName = args.word(1);
  sstring fromName = args.word(2);

  if (intoName.empty() || fromName.empty()) {
    sendTo("Combine which ingot into which?\n\r");
    return;
  }

  TIngot* into = dynamic_cast<TIngot*>(
    searchLinkedListVis(this, intoName.c_str(), stuff));
  TIngot* from = dynamic_cast<TIngot*>(
    searchLinkedListVis(this, fromName.c_str(), stuff));

  if (!combineCheck(this, into, from))
    return;

  if (task)
    stopTask();

  act("You set $p in the fire beside the other bar.", false, this, from, 0,
    TO_CHAR);
  act("$n sets $p in the fire beside another bar.", true, this, from, 0,
    TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_FORGE, 8);

  // The donor is the work: its structure is the clock, and it is the bar the
  // task holds, since it is the one that gets consumed. The target is found
  // again by name at the end -- a bar that has been dropped or sold in the
  // meantime simply ends the job.
  start_task(this, from, nullptr, TASK_COMBINE, intoName.c_str(),
    max(1, static_cast<int>(from->getMaxStructPoints())), in_room, 0, 0, 0);
}
void TBeing::doSmelt(const char* argument) {
  char name_buf[256];

  if (!doesKnowSkill(SKILL_SMELT)) {
    sendTo("You know nothing about smelting.\n\r");
    return;
  }

  strcpy(name_buf, argument);

  if (!*name_buf) {
    sendTo("What is it you intend to smelt?\n\r");
    return;
  }

  TThing* found = searchLinkedListVis(this, name_buf, stuff);
  TObj* obj = dynamic_cast<TObj*>(found);
  if (!obj) {
    sendTo("You need to be carrying that to smelt it.\n\r");
    return;
  }

  // Material is the only test. A key, a shield and a ring all melt the same
  // way, so there is no list of item types here on purpose.
  if (!isMetalMaterial(obj->getMaterial())) {
    act("$p is not made of anything you could melt down.", false, this, obj, 0,
      TO_CHAR);
    return;
  }

  itemTypeT type = obj->itemType();
  if (type == ITEM_INGOT || type == ITEM_RAW_MATERIAL) {
    act("$p is already so much raw metal.", false, this, obj, 0, TO_CHAR);
    return;
  }

  if (task)
    stopTask();

  act("You set $p in the fire and begin working it down.", false, this, obj, 0,
    TO_CHAR);
  act("$n sets $p in the fire.", true, this, obj, 0, TO_ROOM);

  learnFromDoingUnusual(LEARN_UNUSUAL_NORM_LEARN, SKILL_SMELT, 8);

  // Structure is the clock here as everywhere, but nothing damages the item
  // along the way: it is going in the fire regardless, and what the misses
  // cost is the grade of what comes out.
  start_task(this, obj, nullptr, TASK_SMELT, "",
    max(1, static_cast<int>(obj->getMaxStructPoints())), in_room, 0, 0, 0);
}
