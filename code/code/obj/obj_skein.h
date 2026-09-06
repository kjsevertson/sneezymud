//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//      "obj_skein.h" - Worked fibre, the input Sew builds clothing from
//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "obj.h"

// A skein is the ingot's twin on the soft side: fibre pulled out of a worn
// thing and made ready to work again. It remembers how cleanly the weaving
// went and how much came back, and Sew reads that grade to decide how much of
// the carried stats survive into the new piece.
//
// The stats ride in the skein's own affected[] array, which rent persists.
// AC never comes across: armor value belongs to the shape, not the thread.
class TSkein : public TObj {
  private:
    int quality;  // 1-5, from the ratio of landed to missed smelt pulses
    int units;    // tenths of a pound, the same scale commodities use

  public:
    virtual void assignFourValues(int, int, int, int);
    virtual void getFourValues(int*, int*, int*, int*) const;
    virtual sstring statObjInfo() const;
    virtual itemTypeT itemType() const { return ITEM_SKEIN; }

    virtual void lowCheck();
    virtual void describeObjectSpecifics(const TBeing*) const;

    int getSkeinQuality() const;
    void setSkeinQuality(int n);
    int getSkeinUnits() const;
    void setSkeinUnits(int n);

    TSkein();
    TSkein(const TSkein& a);
    TSkein& operator=(const TSkein& a);
    virtual ~TSkein();
};
