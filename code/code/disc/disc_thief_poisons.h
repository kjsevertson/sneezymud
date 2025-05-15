#pragma once

#include "discipline.h"
#include "skills.h"

class CDPoisons : public CDiscipline {
  public:
    CSkill skPoisonWeapons;
    CSkill skToxicity;
    CSkill skHarvestReagent;

    virtual CDPoisons* cloneMe() { return new CDPoisons(*this); }

  private:
};

int get_toxic_plant_vnum(sectorTypeT sector);
