//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//      "obj_skein.cc" - Procedures for skeins
//
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "being.h"
#include "obj_skein.h"

TSkein::TSkein() : TObj(), quality(1), units(0) {}

TSkein::TSkein(const TSkein& a) : TObj(a), quality(a.quality), units(a.units) {}

TSkein& TSkein::operator=(const TSkein& a) {
  if (this == &a)
    return *this;
  TObj::operator=(a);
  quality = a.quality;
  units = a.units;
  return *this;
}

TSkein::~TSkein() {}

int TSkein::getSkeinQuality() const { return quality; }

void TSkein::setSkeinQuality(int n) { quality = max(1, min(5, n)); }

int TSkein::getSkeinUnits() const { return units; }

void TSkein::setSkeinUnits(int n) { units = max(0, n); }

sstring TSkein::statObjInfo() const {
  char buf[256];

  sprintf(buf, "Quality : %d\n\rUnits : %d", getSkeinQuality(),
    getSkeinUnits());

  sstring a(buf);
  return a;
}

void TSkein::assignFourValues(int x1, int x2, int, int) {
  setSkeinQuality(x1);
  setSkeinUnits(x2);
}

void TSkein::getFourValues(int* x1, int* x2, int* x3, int* x4) const {
  *x1 = getSkeinQuality();
  *x2 = getSkeinUnits();
  *x3 = 0;
  *x4 = 0;
}

void TSkein::lowCheck() {
  if (getSkeinQuality() < 1 || getSkeinQuality() > 5)
    vlogf(LOG_LOW, format("Skein (%s) with quality %d outside 1-5.") %
                     getName() % getSkeinQuality());

  if (getSkeinUnits() <= 0)
    vlogf(LOG_LOW,
      format("Skein (%s) with no thread in it.") % getName());

  TObj::lowCheck();
}

// The grade is the thing a smith actually chooses between when picking which
// ingot to forge from, so it goes in the description rather than only in stat.
void TSkein::describeObjectSpecifics(const TBeing* ch) const {
  static const char* const grades[] = {"", "coarse", "uneven", "sound", "fine",
    "flawless"};

  ch->sendTo(format("It is %s thread, and there is enough here for %d units "
                    "of work.\n\r") %
             grades[max(1, min(5, getSkeinQuality()))] % getSkeinUnits());
}
