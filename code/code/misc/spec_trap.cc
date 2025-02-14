#include <vector>
#include <unordered_set>
#include "being.h"
#include "room.h"
#include "low.h"
#include "extern.h"
#include "monster.h"
#include "obj.h"
#include "obj_open_container.h"
#include "obj_trap.h"
#include "obj_arrow.h"
#include "limbs.h"

//// spike trap
int trapBolt(int amt, TTrap* trap, TBeing*, TBeing* vict) {
  //// embed spikes
  std::vector<wearSlotT> validLimbs;
  int maxNumLimbs = trap->getTrapLevel() / 3;
  int numLimbs = ::number((maxNumLimbs / 3), maxNumLimbs);
  int limbDam = ::number(trap->getTrapLevel() / 2, trap->getTrapLevel());
  limbDam = limbDam * (vict->getImmunity(IMMUNE_PIERCE) / 100);
  TObj* bolt = read_object(31349, REAL);

  for (wearSlotT limb; limb < MAX_WEAR; limb++) {
    // put particular checks here, pass through vict
    if (vict->hasPart(limb) && !vict->getStuckIn(limb)) {
      validLimbs.push_back(limb);
    }
  }

  while (numLimbs > 0 && !validLimbs.empty()) {
    unsigned int index = ::number(0, validLimbs.size() - 1);
    wearSlotT limb = validLimbs[index];
    /// do things to the limb here
    if (vict->equipment[limb]) {
      auto* eq = dynamic_cast<TObj*>(vict->equipment[limb]);
      eq->addToStructPoints(-limbDam);
      sstring buf = format("A bolt tears through $N's %s, damaging it!") %
                    vict->equipment[limb];
      act(buf, false, vict, nullptr, nullptr, TO_ROOM, ANSI_YELLOW);
      buf = format("A bolt tears through your %s, damaging it!") %
                    vict->equipment[limb];
      act(buf, false, vict, nullptr, nullptr, TO_VICT, ANSI_YELLOW);
    }
    vict->stickIn(bolt, limb);
    vict->addCurLimbHealth(limb, -limbDam);

    sstring buf = format("A bolt embeds itself in $N's %s!") %
                  vict->describeBodySlot(limb);
    act(buf, false, vict, nullptr, nullptr, TO_ROOM, ANSI_ORANGE);
    buf = format("A bolt embeds itself in your %s!") %
                  vict->describeBodySlot(limb);
    act(buf, false, vict, nullptr, nullptr, TO_VICT, ANSI_ORANGE);

    /// do second part here within if statement
    if (!vict->isTough() && !vict->isImmune(IMMUNE_BLEED, limb)) {
      vict->rawBleed(limb, 250, SILENT_YES, CHECK_IMMUNITY_NO);
      act("Blood begins to flow from the wound!", false, vict, nullptr, nullptr,
        TO_ROOM, ANSI_RED_BOLD);
      act("Blood begins to flow from the wound!", false, vict, nullptr, nullptr,
        TO_CHAR, ANSI_RED_BOLD);
    }
    // back to vector stuff
    --numLimbs;
    if (index != validLimbs.size() - 1) {
      std::swap(validLimbs[index], validLimbs.back());
    }
    validLimbs.pop_back();
  }
  return true;
}

int trapAcid(int amt, TTrap* trap, TBeing*, TBeing* vict) {
  //// eat armor, equip glob of acid, does damage on pulse
  int maxNumLimbs = trap->getTrapLevel() / 3;
  int numLimbs = ::number((maxNumLimbs / 3), maxNumLimbs);
  int limbDam = ::number(trap->getTrapLevel() / 5, trap->getTrapLevel() / 2.5);
  limbDam = limbDam * (vict->getImmunity(IMMUNE_ACID) / 100);

  TObj* glob = read_object(31349, REAL);

  std::vector<wearSlotT> validLimbs;

  for (wearSlotT limb; limb < MAX_WEAR; limb++) {
    // check for specific limb conditions
    if (vict->hasPart(limb) && !vict->equipment[limb] &&
        !vict->getStuckIn(limb)) {
      validLimbs.push_back(limb);
    }
  }

  // Assuming you calculated the number of limbs to damage somehow previously
  // and captured it in an int called `numLimbs` Do this until you've either
  // damaged the max total number of limbs, or run out of valid target limbs
  while (numLimbs > 0 && !validLimbs.empty()) {
    unsigned int index = ::number(0, validLimbs.size() - 1);
    wearSlotT limb = validLimbs[index];

    vict->addCurLimbHealth(limb, -limbDam);
    sstring buf =
      format("Acid splashes onto $N's %s!") % vict->describeBodySlot(limb);
    act(buf, false, vict, nullptr, nullptr, TO_ROOM, ANSI_GREEN_BOLD);
    buf =
      format("Acid splashes onto $N's %s!") % vict->describeBodySlot(limb);
    act(buf, false, vict, nullptr, nullptr, TO_VICT, ANSI_GREEN_BOLD);

    if (!vict->isAgile(0)) {
      vict->equipChar(glob, limb, SILENT_YES);
      buf = format("$N's %s is totally covered in a glob of acid!") %
                    vict->describeBodySlot(limb);
      act(buf, false, vict, nullptr, nullptr, TO_ROOM, ANSI_GREEN_BOLD);
      buf = format("Your %s is totally covered in a glob of acid!") %
                    vict->describeBodySlot(limb);
      act(buf, false, vict, nullptr, nullptr, TO_VICT, ANSI_GREEN_BOLD);
    }

    // Reduce remaining limbs to damage before next loop
    --numLimbs;

    // Remove the limb damaged in this iteration from the vector so it can't be
    // chosen again. It's way more efficient to just pop the last element off a
    // vector, so if we didn't already randomly select the last element, swap
    // the one we chose with the last one then pop it off the back.
    if (index != validLimbs.size() - 1) {
      std::swap(validLimbs[index], validLimbs.back());
    }
    validLimbs.pop_back();
  }
  return true;
}
int trapExplosive(int amt, TTrap* trap, TBeing*, TBeing* vict) {
  //// eat armor, equip glob of acid, does damage on pulse
  int maxNumLimbs = trap->getTrapLevel() / 3;
  int numLimbs = ::number((maxNumLimbs / 3), maxNumLimbs);
  int limbDam = ::number(trap->getTrapLevel() / 5, trap->getTrapLevel() / 2.5);
  limbDam = limbDam * (vict->getImmunity(IMMUNE_ACID) / 100);

  std::vector<wearSlotT> validLimbs;

  for (wearSlotT limb; limb < MAX_WEAR; limb++) {
    // Not sure of all the checks you'd need here
    if (vict->hasPart(limb) && !vict->equipment[limb] &&
        !vict->getStuckIn(limb)) {
      validLimbs.push_back(limb);
    }
  }

  // Assuming you calculated the number of limbs to damage somehow previously
  // and captured it in an int called `numLimbs` Do this until you've either
  // damaged the max total number of limbs, or run out of valid target limbs
  while (numLimbs > 0 && !validLimbs.empty()) {
    unsigned int index = ::number(0, validLimbs.size() - 1);
    wearSlotT limb = validLimbs[index];

    vict->addCurLimbHealth(limb, -limbDam);
    sstring buf =
      format("Acid splashes onto $N's %s!") % vict->describeBodySlot(limb);
    act(buf, false, vict, nullptr, nullptr, TO_ROOM, ANSI_GREEN_BOLD);
    buf =
      format("Acid splashes onto $N's %s!") % vict->describeBodySlot(limb);
    act(buf, false, vict, nullptr, nullptr, TO_VICT, ANSI_GREEN_BOLD);

    TObj* shrapnel = read_object(31349, REAL);
    shrapnel->addObjStat(ITEM_BURNING);
    vict->stickIn(shrapnel, limb);
    act("Flaming chunks of shrapnel explode outward from the trap!", false,
      vict, 0, 0, TO_ROOM);
    act("$n is hit directly by a chunk of debris, which embeds in $n flesh!",
      false, vict, 0, 0, TO_NOTVICT);
    act("You are hit by debris, which embeds into your flesh!", false, vict, 0,
      0, TO_VICT);

    // Reduce remaining limbs to damage before next loop
    --numLimbs;

    // Remove the limb damaged in this iteration from the vector so it can't be
    // chosen again. It's way more efficient to just pop the last element off a
    // vector, so if we didn't already randomly select the last element, swap
    // the one we chose with the last one then pop it off the back.
    if (index != validLimbs.size() - 1) {
      std::swap(validLimbs[index], validLimbs.back());
    }
    validLimbs.pop_back();
  }
  return true;
}

int trapFrost(int amt, TTrap* trap, TBeing*, TBeing* vict) {
  //// eat armor, equip glob of acid, does damage on pulse
  int maxNumLimbs = trap->getTrapLevel() / 3;
  int numLimbs = ::number((maxNumLimbs / 3), maxNumLimbs);
  int limbDam = ::number(trap->getTrapLevel() / 5, trap->getTrapLevel() / 2.5);
  limbDam = limbDam * (vict->getImmunity(IMMUNE_COLD) / 100);

  TObj* icicle = read_object(31349, REAL);

  std::vector<wearSlotT> validLimbs;

  for (wearSlotT limb; limb < MAX_WEAR; limb++) {
    // Not sure of all the checks you'd need here
    if (vict->hasPart(limb) && !vict->equipment[limb] &&
        !vict->getStuckIn(limb)) {
      validLimbs.push_back(limb);
    }
  }

  // Assuming you calculated the number of limbs to damage somehow previously
  // and captured it in an int called `numLimbs` Do this until you've either
  // damaged the max total number of limbs, or run out of valid target limbs
  while (numLimbs > 0 && !validLimbs.empty()) {
    unsigned int index = ::number(0, validLimbs.size() - 1);
    wearSlotT limb = validLimbs[index];
    if (vict->equipment[limb]) {
      auto* eq = dynamic_cast<TObj*>(vict->equipment[limb]);
      eq->addToStructPoints(-limbDam);
      sstring buf =
        format("A shard of ice tears through $N's %s, damaging it!") %
        vict->equipment[limb];
      act(buf, false, vict, nullptr, nullptr, TO_ROOM, ANSI_WHITE_BOLD);
      buf =
        format("A shard of ice tears through your %s, damaging it!") %
        vict->equipment[limb];
      act(buf, false, vict, nullptr, nullptr, TO_VICT, ANSI_WHITE_BOLD);
    } else {
      vict->addCurLimbHealth(limb, -limbDam);
      sstring buf =
        format("A shard of ice tears through $N's %s, damaging it!") %
        vict->describeBodySlot(limb);
      act(buf, false, vict, nullptr, nullptr, TO_ROOM, ANSI_WHITE_BOLD);
      buf =
        format("A shard of ice tears through your %s, damaging it!") %
        vict->describeBodySlot(limb);
      act(buf, false, vict, nullptr, nullptr, TO_VICT, ANSI_WHITE_BOLD);
      if (!vict->isImmune(IMMUNE_COLD, WEAR_BODY)) {
        vict->stickIn(icicle, limb);
        buf = format("A icicle embeds itself in $N's %s!") %
                      vict->describeBodySlot(limb);
        act(buf, false, vict, nullptr, nullptr, TO_ROOM, ANSI_BLUE_BOLD);
        buf = format("An icicle embeds itself in your %s!") %
                      vict->describeBodySlot(limb);
        act(buf, false, vict, nullptr, nullptr, TO_VICT, ANSI_BLUE_BOLD);
      }
    }

    // Reduce remaining limbs to damage before next loop
    --numLimbs;

    // Remove the limb damaged in this iteration from the vector so it can't be
    // chosen again. It's way more efficient to just pop the last element off a
    // vector, so if we didn't already randomly select the last element, swap
    // the one we chose with the last one then pop it off the back.
    if (index != validLimbs.size() - 1) {
      std::swap(validLimbs[index], validLimbs.back());
    }
    validLimbs.pop_back();
  }
  return true;
}
int trapFire(int amt, TTrap* trap, TBeing*, TBeing* vict) {
  std::vector<wearSlotT> validLimbs;
  int level = trap->getTrapLevel();
  int maxNumLimbs = trap->getTrapLevel() / 3;
  int numLimbs = ::number((maxNumLimbs / 3), maxNumLimbs);
  int limbDam = ::number(trap->getTrapLevel() / 5, trap->getTrapLevel() / 2.5);
  limbDam = limbDam * (vict->getImmunity(IMMUNE_PIERCE) / 100);

  for (wearSlotT limb; limb < MAX_WEAR; limb++) {
    // put particular checks here, pass through vict
    if (vict->hasPart(limb) && !vict->isImmune(IMMUNE_HEAT, limb)) {
      validLimbs.push_back(limb);
    }
  }

  while (numLimbs > 0 && !validLimbs.empty()) {
    unsigned int index = ::number(0, validLimbs.size() - 1);
    wearSlotT limb = validLimbs[index];
    /// do things to the limb here
    if (!vict->equipment[limb]) {
      vict->addCurLimbHealth(limb, -limbDam);

      sstring buf = format("Blazing tongues of fire burn $N's %s!") %
                    vict->describeBodySlot(limb);
      act(buf, false, vict, nullptr, nullptr, TO_ROOM, ANSI_ORANGE);
      buf = format("Blazing tongues of fire burn your %s!") %
                    vict->describeBodySlot(limb);
      act(buf, false, vict, nullptr, nullptr, TO_VICT, ANSI_ORANGE);
      if (!vict->isImmune(IMMUNE_HEAT, limb) && !vict->isAgile(0)) {
        int burnLev = ::number(1, level / 3);
        affectedData af1;
        af1.type = SPELL_FIRE_BREATH;
        af1.level = level;
        af1.duration = (3 + (level / 2)) * Pulse::UPDATES_PER_MUDHOUR;
        af1.location = APPLY_IMMUNITY;
        af1.modifier = IMMUNE_HEAT;
        af1.modifier2 = burnLev;
        af1.bitvector = 0;
              buf = format("$N's %s begins to crack and char from the heat!") %
                    vict->describeBodySlot(limb);
      act(buf, false, vict, nullptr, nullptr, TO_ROOM, ANSI_GRAY);
      buf = format("Your %s begins to crack and char from the heat!") %
                    vict->describeBodySlot(limb);
      }
      if (!vict->isPerceptive() && limb == WEAR_HEAD &&
          vict->hasDisease(DISEASE_EYEBALL)) {
        affectedData af;
        af.type = AFFECT_DISEASE;
        af.level = 0;  // has to be 0 for doctor to treat
        af.duration = PERMANENT_DURATION;
        af.modifier = DISEASE_EYEBALL;
        af.location = APPLY_NONE;
        af.bitvector = AFF_BLIND;
        vict->affectTo(&af);
        vict->rawBlind((level), af.duration, SAVE_NO);
        act("$N is blinded by the heat!", false, vict, nullptr, nullptr,
          TO_ROOM, ANSI_YELLOW_BOLD);
        act("The world goes black in a flash as you are blinded by the heat!",
          false, vict, nullptr, nullptr, TO_VICT, ANSI_YELLOW_BOLD);
      }
      // back to vector stuff
      --numLimbs;
      if (index != validLimbs.size() - 1) {
        std::swap(validLimbs[index], validLimbs.back());
      }
      validLimbs.pop_back();
    }
  }
  return true;
}
int trapPower(int amt, TTrap* trap, TBeing*, TBeing* vict) {
  std::vector<wearSlotT> validLimbs;
  int level = trap->getTrapLevel();
  int maxNumLimbs = trap->getTrapLevel() / 3;
  int numLimbs = ::number((maxNumLimbs / 3), maxNumLimbs);
  int limbDam = ::number(trap->getTrapLevel() / 5, trap->getTrapLevel() / 2.5);
  limbDam = limbDam * (vict->getImmunity(IMMUNE_ENERGY) / 100);

  for (wearSlotT limb; limb < MAX_WEAR; limb++) {
    // put particular checks here, pass through vict
    if (vict->hasPart(limb) && !vict->isImmune(IMMUNE_ENERGY, limb)) {
      validLimbs.push_back(limb);
    }
  }

  while (numLimbs > 0 && !validLimbs.empty()) {
    unsigned int index = ::number(0, validLimbs.size() - 1);
    wearSlotT limb = validLimbs[index];
    /// do things to the limb here
    if (!vict->equipment[limb]) {
      limbDam =+ limbDam; ;

      sstring buf = format("Bolts of arcane energy surge through $N's %s!") %
                    vict->describeBodySlot(limb);
      act(buf, false, vict, nullptr, nullptr, TO_ROOM, ANSI_PURPLE_BOLD);
      buf = format("Bolts of arcane energy surge through your %s!") %
                    vict->describeBodySlot(limb);
      act(buf, false, vict, nullptr, nullptr, TO_VICT, ANSI_PURPLE_BOLD);
      if (!vict->isImmune(IMMUNE_ENERGY, limb) && (!vict->isWise() || !vict->isIntelligent())) {
        int zapLev = ::number(1, level / 3);
        affectedData af1;
        af1.type = SPELL_ATOMIZE;
        af1.level = level;
        af1.duration = (3 + (level / 2)) * Pulse::UPDATES_PER_MUDHOUR;
        af1.location = APPLY_IMMUNITY;
        af1.modifier = IMMUNE_ENERGY;
        af1.modifier2 = zapLev;
        af1.bitvector = 0;        
      }
      
      // back to vector stuff
      --numLimbs;
      if (index != validLimbs.size() - 1) {
        std::swap(validLimbs[index], validLimbs.back());
      }
      validLimbs.pop_back();
    }
  }
  int headStr = vict->getCurLimbHealth(WEAR_HEAD);
  genericCurse(nullptr, vict, 50, SPELL_CURSE);
  if (headStr <= limbDam) {
  affectedData aff;
  aff.type = SPELL_PARALYZE;
  aff.level = level;
  aff.location = APPLY_NONE;
  aff.bitvector = AFF_PARALYSIS;
  aff.modifier = 0;

  // balance notes, each "duration" is a complete round out of action
  // to compare to other spell damages, we assume mob does 1.20 * lev
  // dam per round
  // clerics ought to be doing 1.60 * lev dam per round.
  // theoretically, that means paralyze ought to be 4/3 * difficulty
  // modifier of 100/60 = 20/9 = 2.25
  // anyway, lets bump it up to 3 rounds
  aff.duration = level/3;
  }
  return true;
}

/*


int trapBolt(int amt, TBeing*, TBeing* vict) {}
//// embed bolt
int trapPebble(int amt, TBeing*, TBeing* vict) {}
//// limb damage, apply bruise
int trapPower(int amt, TBeing*, TBeing* vict) {}
//// apply curse
*/