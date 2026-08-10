//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "obj_base_weapon.h"

// The ammunition families, each in a long and a short version - stalks
// (arrows), quarrels, darts and pellets.  A bow is locked to exactly one
// of these values and refuses anything else, so the same table names both ends
// of the pair: what an arrow is, and what a bow will fire.
struct ArrowTypeNames {
    const char* singular;  // "Hunting Arrow"
    const char* plural;    // "Hunting arrows"
};

[[nodiscard]] const ArrowTypeNames& arrowTypeNames(unsigned int arrowType);

// The eight values decompose into four families of two: even is the long
// version, odd the short one.  Names follow lib/help/_builder/arrows, which is
// what builders read when authoring ammunition.
enum arrowFamilyT {
  ARROW_FAM_STALK = 0,
  ARROW_FAM_QUARREL,
  ARROW_FAM_DART,
  ARROW_FAM_PELLET,
  ARROW_FAM_COUNT,
};

// assignFourValues hands setArrowType four bits, so 0-15 can arrive, but only
// two types per family are named.  Anything above this would send arrowFamily
// past the end of the per-family tables.
inline constexpr unsigned int MAX_ARROW_TYPE = 2 * ARROW_FAM_COUNT - 1;

[[nodiscard]] constexpr arrowFamilyT arrowFamily(unsigned int arrowType) {
  return static_cast<arrowFamilyT>(arrowType >> 1);
}

[[nodiscard]] constexpr bool arrowIsLong(unsigned int arrowType) {
  return !(arrowType & 1);
}

class TArrow : public TBaseWeapon {
  private:
    // Percentage damage bonus stamped on at launch, from the archer's strength
    // and the bow's draw.  Runtime-only flight state: deliberately absent from
    // assignFourValues/getFourValues so it never reaches the database.
    int launchPower = 0;

    unsigned char arrowType;
    unsigned char arrowHead;
    unsigned int arrowHeadMat;
    unsigned int arrowFlags;
    unsigned int trap_level;
    doorTrapT trap_dam_type;

  public:
    virtual void assignFourValues(int, int, int, int);
    virtual void getFourValues(int*, int*, int*, int*) const;
    virtual sstring statObjInfo() const;
    virtual itemTypeT itemType() const { return ITEM_ARROW; }
    virtual int suggestedPrice() const;

    int getTrapLevel() const;
    void setTrapLevel(int r);
    doorTrapT getTrapDamType() const;
    void setTrapDamType(doorTrapT r);
    int getTrapDamAmount() const;

    unsigned char getArrowType() const;
    void setArrowType(unsigned int);
    unsigned char getArrowHead() const;
    void setArrowHead(unsigned char);
    int hasFlag(const int) const;
    unsigned char getArrowHeadMat() const;
    void setArrowHeadMat(unsigned char);

    [[nodiscard]] int getLaunchPower() const { return launchPower; }
    void setLaunchPower(int n) { launchPower = n; }
    unsigned short getArrowFlags() const;
    void setArrowFlags(unsigned short);
    void remArrowFlags(unsigned short);
    bool isArrowFlag(unsigned short);
    void addArrowFlags(unsigned short);

    virtual spellNumT getWtype(int which = -1) const;
    virtual void evaluateMe(TBeing*) const;
    virtual bool engraveMe(TBeing*, TMonster*, bool);
    virtual void bloadBowArrow(TBeing*, TThing*);
    virtual int throwMe(TBeing*, dirTypeT, const char*);
    virtual int putMeInto(TBeing*, TOpenContainer*);
    virtual sstring compareMeAgainst(TBeing*, TObj*);
    virtual void changeObjValue3(TBeing*);
    virtual void changeObjValue4(TBeing*);
    virtual sstring displayFourValues();
    virtual bool sellMeCheck(TBeing*, TMonster*, int, int) const;
    virtual int trapMe(TBeing*, const char*);
    virtual int disarmMe(TBeing*);

    TArrow();
    TArrow(const TArrow& a);
    TArrow& operator=(const TArrow& a);
    virtual ~TArrow();
};
