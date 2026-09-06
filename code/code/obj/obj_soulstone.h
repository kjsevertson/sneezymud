//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//      "obj_soulstone.h" - A stone that holds souls, fuel for Bolster
//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "obj.h"

// A soulstone holds charges harvested from corpses by Rites and spends them on
// Bolster. Its Level, 1 to 10, governs both how much a single Rites yields and
// how far it can be grown.
//
// All ten Levels share one vnum, with Level and charges in the val fields --
// rent persists all four, so both survive storage. A stone rises a Level when
// it holds level x 100 charges and never falls: the threshold is checked only
// on gain. Spending still costs progress, since spent charges have to be
// earned again to reach the next threshold, which is the tension between using
// a stone and growing it.
class TSoulstone : public TObj {
  private:
    int soulLevel;
    int charges;

  public:
    virtual void assignFourValues(int, int, int, int);
    virtual void getFourValues(int*, int*, int*, int*) const;
    virtual sstring statObjInfo() const;
    virtual itemTypeT itemType() const { return ITEM_SOULSTONE; }

    virtual void lowCheck();
    virtual void describeObjectSpecifics(const TBeing*) const;

    int getSoulLevel() const;
    void setSoulLevel(int n);
    int getCharges() const;
    void setCharges(int n);

    // Charges in, threshold checked, Level raised if it was crossed. Returns
    // true if the stone gained a Level, which the caller announces.
    bool addCharges(int n);

    // Charges out. False if the stone does not hold that many.
    bool spendCharges(int n);

    int chargeCap() const;
    int nextThreshold() const;

    TSoulstone();
    TSoulstone(const TSoulstone& a);
    TSoulstone& operator=(const TSoulstone& a);
    virtual ~TSoulstone();
};
