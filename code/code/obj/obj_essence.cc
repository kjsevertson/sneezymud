//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//      "obj_essence.cc" - Procedures for essences
//
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "being.h"
#include "obj_essence.h"

const int ESSENCE_MAX_QUALITY = 10;
const int ESSENCE_CHARGES_PER_QUALITY = 10;

namespace {
  struct EssenceApply {
      int apply;
      const char* name;
  };

  // Six primary stats, six secondary, three pools, two combat, two
  // perception. CAN_BE_SEEN and VISION read as behavioral but are plain
  // additive bonuses in affectModify, which is the test that matters.
  const EssenceApply essenceApplies[] = {
    {APPLY_STR, "strength"},
    {APPLY_INT, "intelligence"},
    {APPLY_WIS, "wisdom"},
    {APPLY_DEX, "dexterity"},
    {APPLY_CON, "constitution"},
    {APPLY_KAR, "karma"},
    {APPLY_BRA, "brawn"},
    {APPLY_AGI, "agility"},
    {APPLY_FOC, "focus"},
    {APPLY_SPE, "speed"},
    {APPLY_PER, "perception"},
    {APPLY_CHA, "charisma"},
    {APPLY_HIT, "hit points"},
    {APPLY_MANA, "mana"},
    {APPLY_MOVE, "movement"},
    {APPLY_HITROLL, "hitroll"},
    {APPLY_DAMROLL, "damroll"},
    {APPLY_CAN_BE_SEEN, "visibility"},
    {APPLY_VISION, "vision"},
  };
}  // namespace

bool isEssenceApply(int apply) {
  for (const auto& entry : essenceApplies)
    if (entry.apply == apply)
      return true;

  return false;
}

const char* essenceApplyName(int apply) {
  for (const auto& entry : essenceApplies)
    if (entry.apply == apply)
      return entry.name;

  return "nothing";
}

TEssence::TEssence() : TObj(), applyType(APPLY_NONE), quality(1), charges(0) {}

TEssence::TEssence(const TEssence& a) :
  TObj(a), applyType(a.applyType), quality(a.quality), charges(a.charges) {}

TEssence& TEssence::operator=(const TEssence& a) {
  if (this == &a)
    return *this;
  TObj::operator=(a);
  applyType = a.applyType;
  quality = a.quality;
  charges = a.charges;
  return *this;
}

TEssence::~TEssence() {}

int TEssence::getApplyType() const { return applyType; }

void TEssence::setApplyType(int n) { applyType = n; }

int TEssence::getQuality() const { return quality; }

void TEssence::setQuality(int n) {
  quality = max(1, min(ESSENCE_MAX_QUALITY, n));
}

int TEssence::getCharges() const { return charges; }

void TEssence::setCharges(int n) { charges = max(0, n); }

// What this essence must hold to deepen, or 0 at the top.
int TEssence::nextThreshold() const {
  if (quality >= ESSENCE_MAX_QUALITY)
    return 0;

  return quality * ESSENCE_CHARGES_PER_QUALITY;
}

bool TEssence::addCharges(int n) {
  if (n <= 0)
    return false;

  int before = quality;
  charges += n;

  while (quality < ESSENCE_MAX_QUALITY && charges >= nextThreshold())
    quality++;

  return quality > before;
}

sstring TEssence::statObjInfo() const {
  char buf[256];

  sprintf(buf, "Apply : %s (%d)\n\rQuality : %d\n\rCharges : %d",
    essenceApplyName(getApplyType()), getApplyType(), getQuality(),
    getCharges());

  sstring a(buf);
  return a;
}

void TEssence::assignFourValues(int x1, int x2, int x3, int) {
  setApplyType(x1);
  setQuality(x2);
  setCharges(x3);
}

void TEssence::getFourValues(int* x1, int* x2, int* x3, int* x4) const {
  *x1 = getApplyType();
  *x2 = getQuality();
  *x3 = getCharges();
  *x4 = 0;
}

void TEssence::lowCheck() {
  if (!isEssenceApply(getApplyType()))
    vlogf(LOG_LOW, format("Essence (%s) carrying apply %d, which is not one "
                          "an essence may hold.") %
                     getName() % getApplyType());

  if (getQuality() < 1 || getQuality() > ESSENCE_MAX_QUALITY)
    vlogf(LOG_LOW, format("Essence (%s) at quality %d, outside 1-10.") %
                     getName() % getQuality());

  TObj::lowCheck();
}

void TEssence::describeObjectSpecifics(const TBeing* ch) const {
  ch->sendTo(format("It is %s essence of quality %d, and would write +%d.\n\r") %
             essenceApplyName(getApplyType()) % getQuality() % getQuality());

  if (nextThreshold())
    ch->sendTo(format("It holds %d charges, and deepens at %d.\n\r") %
               getCharges() % nextThreshold());
  else
    ch->sendTo(format("It holds %d charges and can deepen no further.\n\r") %
               getCharges());
}
