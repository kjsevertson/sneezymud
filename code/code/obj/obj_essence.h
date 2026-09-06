//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//      "obj_essence.h" - A stat pulled out of an item and made portable
//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "obj.h"

// An essence is one apply, held loose. Distill puts a destroyed item's stats
// into essences; Infuse writes them onto another item.
//
// Quality alone decides what Infuse can write -- a Quality 3 essence writes
// +3 -- and charges exist only to grow the Quality, never entering the
// application. An essence deepens at quality x 10 charges and caps at 10, so
// +10 is the largest bonus this system can ever write.
//
// The threshold is checked only on gain, so an essence never loses Quality.
// Unlike a soulstone, Quality does nothing to what Distill deposits: the same
// item yields the same charges into a Quality 1 essence as a Quality 9 one,
// which is what makes each Quality strictly harder to reach than the last.
class TEssence : public TObj {
  private:
    int applyType;  // an applyTypeT, held as int for the val fields
    int quality;    // 1-10, what Infuse writes
    int charges;    // progress toward the next Quality, nothing more

  public:
    virtual void assignFourValues(int, int, int, int);
    virtual void getFourValues(int*, int*, int*, int*) const;
    virtual sstring statObjInfo() const;
    virtual itemTypeT itemType() const { return ITEM_ESSENCE; }

    virtual void lowCheck();
    virtual void describeObjectSpecifics(const TBeing*) const;

    int getApplyType() const;
    void setApplyType(int n);
    int getQuality() const;
    void setQuality(int n);
    int getCharges() const;
    void setCharges(int n);

    // Charges in, Quality raised if the threshold was crossed. True if it
    // deepened, which the caller announces.
    bool addCharges(int n);

    int nextThreshold() const;

    TEssence();
    TEssence(const TEssence& a);
    TEssence& operator=(const TEssence& a);
    virtual ~TEssence();
};

// The nineteen applies whose whole meaning is one signed magnitude, and so the
// only ones an essence can carry. APPLY_ARMOR is deliberately absent: AC
// belongs to the body, owned by the tier ladder and Bolster, and making it
// fungible with STR would have both halves of the system writing one field.
[[nodiscard]] bool isEssenceApply(int apply);

// Display name for an apply an essence can hold.
[[nodiscard]] const char* essenceApplyName(int apply);
