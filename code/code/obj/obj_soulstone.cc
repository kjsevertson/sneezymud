//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//      "obj_soulstone.cc" - Procedures for soulstones
//
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "being.h"
#include "obj_soulstone.h"

const int SOULSTONE_MAX_LEVEL = 10;
const int SOULSTONE_CHARGES_PER_LEVEL = 100;

TSoulstone::TSoulstone() : TObj(), soulLevel(1), charges(0) {}

TSoulstone::TSoulstone(const TSoulstone& a) :
  TObj(a), soulLevel(a.soulLevel), charges(a.charges) {}

TSoulstone& TSoulstone::operator=(const TSoulstone& a) {
  if (this == &a)
    return *this;
  TObj::operator=(a);
  soulLevel = a.soulLevel;
  charges = a.charges;
  return *this;
}

TSoulstone::~TSoulstone() {}

int TSoulstone::getSoulLevel() const { return soulLevel; }

void TSoulstone::setSoulLevel(int n) {
  soulLevel = max(1, min(SOULSTONE_MAX_LEVEL, n));
}

int TSoulstone::getCharges() const { return charges; }

void TSoulstone::setCharges(int n) { charges = max(0, min(chargeCap(), n)); }

// Level 10 holds a thousand and rises no further.
int TSoulstone::chargeCap() const {
  return SOULSTONE_MAX_LEVEL * SOULSTONE_CHARGES_PER_LEVEL;
}

// What this stone must hold to reach its next Level, or 0 at the top.
int TSoulstone::nextThreshold() const {
  if (soulLevel >= SOULSTONE_MAX_LEVEL)
    return 0;

  return soulLevel * SOULSTONE_CHARGES_PER_LEVEL;
}

bool TSoulstone::addCharges(int n) {
  if (n <= 0)
    return false;

  int before = soulLevel;
  setCharges(charges + n);

  // Checked only on gain, so a stone never falls back a Level when it is
  // spent -- but the spent charges do have to be earned again to cross the
  // next threshold.
  while (soulLevel < SOULSTONE_MAX_LEVEL && charges >= nextThreshold())
    soulLevel++;

  return soulLevel > before;
}

bool TSoulstone::spendCharges(int n) {
  if (n <= 0 || charges < n)
    return false;

  charges -= n;
  return true;
}

sstring TSoulstone::statObjInfo() const {
  char buf[256];

  sprintf(buf, "Soulstone level : %d\n\rCharges : %d\n\rNext level at : %d",
    getSoulLevel(), getCharges(), nextThreshold());

  sstring a(buf);
  return a;
}

void TSoulstone::assignFourValues(int x1, int x2, int, int) {
  setSoulLevel(x1);
  setCharges(x2);
}

void TSoulstone::getFourValues(int* x1, int* x2, int* x3, int* x4) const {
  *x1 = getSoulLevel();
  *x2 = getCharges();
  *x3 = 0;
  *x4 = 0;
}

void TSoulstone::lowCheck() {
  if (getSoulLevel() < 1 || getSoulLevel() > SOULSTONE_MAX_LEVEL)
    vlogf(LOG_LOW, format("Soulstone (%s) at level %d, outside 1-10.") %
                     getName() % getSoulLevel());

  if (getCharges() > chargeCap())
    vlogf(LOG_LOW, format("Soulstone (%s) holding %d charges, over cap.") %
                     getName() % getCharges());

  TObj::lowCheck();
}

// All ten Levels share one vnum, so a Level 1 and a Level 10 stone look alike
// until something says otherwise. This is that something.
void TSoulstone::describeObjectSpecifics(const TBeing* ch) const {
  ch->sendTo(format("It is a soulstone of level %d, holding %d charges.\n\r") %
             getSoulLevel() % getCharges());

  if (nextThreshold())
    ch->sendTo(format("It will deepen at %d.\n\r") % nextThreshold());
  else
    ch->sendTo("It will hold no more than it does.\n\r");
}
