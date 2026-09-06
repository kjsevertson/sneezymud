//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//      "obj_ingot.cc" - Procedures for ingots
//
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "being.h"
#include "obj_ingot.h"

TIngot::TIngot() : TObj(), quality(1), units(0) {}

TIngot::TIngot(const TIngot& a) : TObj(a), quality(a.quality), units(a.units) {}

TIngot& TIngot::operator=(const TIngot& a) {
  if (this == &a)
    return *this;
  TObj::operator=(a);
  quality = a.quality;
  units = a.units;
  return *this;
}

TIngot::~TIngot() {}

int TIngot::getIngotQuality() const { return quality; }

void TIngot::setIngotQuality(int n) { quality = max(1, min(5, n)); }

int TIngot::getIngotUnits() const { return units; }

void TIngot::setIngotUnits(int n) { units = max(0, n); }

sstring TIngot::statObjInfo() const {
  char buf[256];

  sprintf(buf, "Quality : %d\n\rUnits : %d", getIngotQuality(),
    getIngotUnits());

  sstring a(buf);
  return a;
}

void TIngot::assignFourValues(int x1, int x2, int, int) {
  setIngotQuality(x1);
  setIngotUnits(x2);
}

void TIngot::getFourValues(int* x1, int* x2, int* x3, int* x4) const {
  *x1 = getIngotQuality();
  *x2 = getIngotUnits();
  *x3 = 0;
  *x4 = 0;
}

void TIngot::lowCheck() {
  if (getIngotQuality() < 1 || getIngotQuality() > 5)
    vlogf(LOG_LOW, format("Ingot (%s) with quality %d outside 1-5.") %
                     getName() % getIngotQuality());

  if (getIngotUnits() <= 0)
    vlogf(LOG_LOW,
      format("Ingot (%s) with no metal in it.") % getName());

  TObj::lowCheck();
}

// The grade is the thing a smith actually chooses between when picking which
// ingot to forge from, so it goes in the description rather than only in stat.
void TIngot::describeObjectSpecifics(const TBeing* ch) const {
  static const char* const grades[] = {"", "crude", "rough", "sound", "fine",
    "flawless"};

  ch->sendTo(format("It is %s metal, and there is enough here for %d units of "
                    "work.\n\r") %
             grades[max(1, min(5, getIngotQuality()))] % getIngotUnits());
}
