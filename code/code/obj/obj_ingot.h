//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//      "obj_ingot.h" - Smelted metal, the input Forge builds armor from
//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "obj.h"

// An ingot remembers two things about the item it was smelted from: how
// cleanly the work went, and how much metal came back. Quality is not a
// property of the metal -- the same steel smelted twice by the same smith
// grades differently -- it is a record of the smelting, and Forge reads it to
// decide how much of the carried stats survive into the new piece.
//
// The stats themselves ride in the ingot's own affected[] array, which rent
// persists, so a smelted item's affects can outlive it in storage. AC never
// comes across: armor value belongs to the shape, not the metal, and Forge
// projects its own.
class TIngot : public TObj {
  private:
    int quality;  // 1-5, from the ratio of landed to missed smelt pulses
    int units;    // tenths of a pound, the same scale commodities use

  public:
    virtual void assignFourValues(int, int, int, int);
    virtual void getFourValues(int*, int*, int*, int*) const;
    virtual sstring statObjInfo() const;
    virtual itemTypeT itemType() const { return ITEM_INGOT; }

    virtual void lowCheck();
    virtual void describeObjectSpecifics(const TBeing*) const;

    int getIngotQuality() const;
    void setIngotQuality(int n);
    int getIngotUnits() const;
    void setIngotUnits(int n);

    TIngot();
    TIngot(const TIngot& a);
    TIngot& operator=(const TIngot& a);
    virtual ~TIngot();
};
