
//
//      SneezyMUD - All rights reserved, SneezyMUD Coding Team
//      "obj_LOW.cc" - routines related to checking stats on objects.
//
//////////////////////////////////////////////////////////////////////////

/*-------------------------------------------------------------------
  New LOW classification rules for ARMOR

  See obj_low.cc for description
 ------------------------------------------------------------------*/

#pragma once

enum Tier {
  // equipment
  Tier_Clothing = 0,
  Tier_Light,
  Tier_Medium,
  Tier_Heavy,
  Tier_Jewelry,

  // weapons
  Tier_Common,
  Tier_Simple,
  Tier_Martial,

  Tier_Max
};

// AC and structure both derive from one number: the level of the mob that
// loads the gear. Each armor tier sits at a fixed point on that scale, and a
// tier move is a rescale between two of these -- see the tier table in
// docs/superpowers/specs/2026-08-23-gear-augmentation-design.md.
//
// Demotion rescales the item's own level rather than snapping it to the tier's
// number, so a piece that loaded off a level 70 mob keeps what made it good --
// but only as far as the rung it lands on will hold. Subtracting absolutely is
// not the same as staying in range: the ceiling falls faster than the ratio
// does, so an uncapped demotion arrives above the tier's own best, with the
// class restrictions shed on the way down. Demotion therefore caps at the load
// level here; promotion caps at the skill ceiling below, which is the lower
// number because it holds down skills that *add* value.
[[nodiscard]] constexpr double getTierLoadLevel(Tier tier) {
  switch (tier) {
    case Tier_Heavy:
      return 60.0;
    case Tier_Medium:
      return 50.0;
    case Tier_Light:
      return 40.0;
    case Tier_Clothing:
      return 30.0;
    default:
      return 0.0;
  }
}

// The rung a piece's armor level alone puts it on: the lowest one whose load
// level can still hold it. A level 55 piece is past medium's 50, so heavy is
// the only rung that fits it, whatever its flags say.
[[nodiscard]] constexpr Tier tierForArmorLevel(double level) {
  if (level > getTierLoadLevel(Tier_Medium))
    return Tier_Heavy;
  if (level > getTierLoadLevel(Tier_Light))
    return Tier_Medium;
  if (level > getTierLoadLevel(Tier_Clothing))
    return Tier_Light;
  return Tier_Clothing;
}

enum PointType {
  PointType_All = 0,
  PointType_Stats,
  PointType_Main,

  PointType_Max
};

// constants used to try to approximate armor vs stats
#define low_acPerHitrate (25.0 / 3.0)  // ac to affect hit rate by 1%
#define low_statValue (0.25)           // change to hit rate for 1 stat point
#define low_acModifier \
  (0.25)  // inflation multiplier for stat costs (used for more than combat,
          // etc)
#define low_acPerLevel (25.0)  // the base AC you get per item level?
#define low_exchangeRate              \
  (low_acPerHitrate * low_statValue * \
    low_acModifier)  // cost in ac for 1 stat point

// base class for TObj
class ObjectEvaluator {
  public:
    ObjectEvaluator(const TObj* o);
    virtual ~ObjectEvaluator(){};

    sstring getTierString();
    int getPointValue(PointType type = PointType_All);
    double getLoadLevel(PointType type = PointType_All);

    // Public because the augmentation skills move items between tiers and so
    // have to ask what tier an item is on. It stays virtual and read-only.
    virtual Tier getTier() = 0;

  private:
    int m_stat;
    const TObj* m_obj;
    bool m_gotStats;

    int getStatPointsRaw();
    // int getStructPointsRaw();  // struct we want to move off of level calc
    // and into weight (so its shown in value)

  protected:
    virtual int getMainPointsRaw() = 0;
    virtual bool IgnoreApply(applyTypeT t) { return false; }
};

// class for TBaseClothing
class ArmorEvaluator : public ObjectEvaluator {
  public:
    ArmorEvaluator(const TBaseClothing* o);
    virtual ~ArmorEvaluator(){};

    virtual Tier getTier();

  private:
    const TBaseClothing* m_clothing;
    int m_main;
    bool m_gotMain;

  protected:
    virtual int getMainPointsRaw();
    virtual bool IgnoreApply(applyTypeT t) { return t == APPLY_ARMOR; }
};

// class for TBaseWeapon
class WeaponEvaluator : public ObjectEvaluator {
  public:
    WeaponEvaluator(const TBaseWeapon* o);
    virtual ~WeaponEvaluator(){};

  private:
    const TBaseWeapon* m_weap;
    int m_main;

  protected:
    virtual int getMainPointsRaw();
    virtual Tier getTier();
};
