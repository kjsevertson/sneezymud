#include "handler.h"
#include "being.h"
#include "combat.h"
#include "obj_general_weapon.h"
#include "materials.h"
#include "skills.h"

#include "handler.h"
#include "being.h"
#include "combat.h"
#include "obj_base_weapon.h"

int TBeing::doStab(const char* argument, TBeing* vict) {
  int rc;
  TBeing* victim;
  const int STAB_MOVE = 3;

  if (checkBusy()) {
    return false;
  }

  // Ensure player even knows the skill before continuing
  if (!doesKnowSkill(SKILL_STABBING)) {
    sendTo(
      "You wouldn't even know where to begin in executing that maneuver.\n\r");
    return false;
  }

  if (!(victim = vict)) {
    if (!(victim = get_char_room_vis(this, argument))) {
      if (!(victim = fight())) {
        sendTo("Hit whom?\n\r");
        return false;
      }
    }
  }
  if (!sameRoom(*victim)) {
    sendTo("That person isn't around.\n\r");
    return false;
  }

  auto* weapon = dynamic_cast<TGenWeapon*>(heldInPrimHand());

  // Ensure this isn't a peaceful room
  if (checkPeaceful("You feel too peaceful to contemplate violence.\n\r"))
    return false;

  // Make sure the player has enough vitality to use the skill
  if (getMove() < STAB_MOVE) {
    sendTo("You don't have the vitality to make the move!\n\r");
    return false;
  }

  // Prevent players from attacking immortals
  if (victim->isImmortal() || IS_SET(victim->specials.act, ACT_IMMORTAL)) {
    sendTo("Attacking an immortal would be a bad idea.\n\r");
    return false;
  }

  // Prevent players from attacking themselves
  if (victim == this) {
    sendTo("Do you REALLY want to kill yourself?...\n\r");
    return false;
  }

  // Avoid players attacking un-harmable victims
  if (noHarmCheck(victim))
    return false;

  // Limit players from using this while mounted
  if (riding) {
    sendTo("You can't perform that attack while mounted!\n\r");
    return false;
  }

  // Ensure the player has a weapon equipped
  if (!weapon) {
    sendTo(
      "You need to hold a weapon in your primary hand to make this a "
      "success.\n\r");
    return false;
  }
  if (!weapon->canStab()) {
    act("You can't use $o to stab.", false, this, weapon, NULL, TO_CHAR);
    return FALSE;
  }
  // Only consume vitality for mortals
  if (!(isImmortal() || IS_SET(specials.act, ACT_IMMORTAL)))
    addToMove(-STAB_MOVE);

  int skillValue = getSkillValue(SKILL_STABBING);
  int successfulHit = specialAttack(victim, SKILL_STABBING);
  int successfulSkill = bSuccess(skillValue, SKILL_STABBING);

  // Success use case
  if (!victim->awake() || (successfulHit && successfulSkill &&
                            successfulHit != GUARANTEED_FAILURE)) {
    rc = slamSuccess(victim);
  }
  // Fail use case
  else {
    rc = slamFail(victim);
  }

  if (rc)
    addSkillLag(SKILL_STABBING, rc);

  if (IS_SET_DELETE(rc, DELETE_VICT)) {
    if (vict)
      return rc;
    delete victim;
    victim = nullptr;
    REM_DELETE(rc, DELETE_VICT);
  }

  if (IS_SET_DELETE(rc, DELETE_THIS)) {
    return DELETE_THIS;
  }

  return rc;
}

int TBeing::stabSuccess(TBeing* victim) {
  int skillLevel = getSkillLevel(SKILL_STABBING);
  int dam = getSkillDam(victim, SKILL_STABBING, skillLevel,
    getAdvLearning(SKILL_STABBING));
  auto* weapon = dynamic_cast<TGenWeapon*>(heldInPrimHand());

  // Scaling damage here due to the limitations of getSkillDam and how it treats
  // skills learned at low levels This formula is designed to allow the damage
  // to scale up to 0.75% of max hp to have some effectiveness against high
  // level opponents, while dealing a respectable amount of damage to lower
  // level enemies
  static const std::map<int, float> scalingDamageConstants = {{10, 0.15},
    {20, 0.08}, {30, 0.05}, {40, 0.03}, {50, 0.02}};

  // Default value for higher level enemies
  float scalingConstant = 0.0075;

  // For enemies level 1-50, retrieving
  for (const auto& damageConstant : scalingDamageConstants) {
    if (victim->GetMaxLevel() <= damageConstant.first) {
      scalingConstant = damageConstant.second;
      break;
    }
  }

  // Apply the scaling constant
  dam = max((int)(victim->hitLimit() * scalingConstant), dam);

  wearSlotT limb = victim->getPartHit(this, false);
  int limbdam = (weapon->getCurSharp() / 47) + (this->GetMaxLevel() / 22);
  if (dynamic_cast<TObj*>(weapon)->isObjStat(ITEM_SPIKED)) {
    limbdam = +1;
  }

  // Send description text to players in the room
  if (!victim->isLimbFlags(limb, PART_MISSING) && !victim->isUndead() &&
      !victim->isLimbFlags(limb, IMMUNE_BLEED)) {
    int duration = (this->GetMaxLevel() * 3 + 200);
    victim->rawBleed(limb, duration, SILENT_YES, CHECK_IMMUNITY_NO);
    victim->hurtLimb(limbdam, limb);
    act("You puncture $N's %s with your $o, leaving a bloody gash!", false,
      this, weapon, victim, TO_CHAR);
    act("$n punctures $N's %s with their $o, leaving a bloody gash!", false,
      this, weapon, victim, TO_CHAR);
    act("$n punctures your $s with their $o, leaving a bloody gash!!", false,
      this, weapon, victim, TO_CHAR);
  } else if (!victim->isLimbFlags(limb, PART_MISSING) &&
             victim->isLimbFlags(limb, IMMUNE_BLEED)) {
    act("You puncture $N's %s with your $o, leaving a gaping hole!", false,
      this, weapon, victim, TO_CHAR);
    act("$n punctures $N's %s with their $o, leaving a gaping hole!", false,
      this, weapon, victim, TO_CHAR);
    act("$n punctures your $s with their $o, leaving a gaping hole!!", false,
      this, weapon, victim, TO_CHAR);

  } else {
    act("You stab $N's %s with your $o, leaving a big wound!", false, this,
      weapon, victim, TO_CHAR);
    act("$n stabs $N's %s with their $o, leaving a big wound!", false, this,
      weapon, victim, TO_CHAR);
    act("$n stab your $s with their $o, leaving a big wound!!", false, this,
      weapon, victim, TO_CHAR);
  }
  if (weapon->isObjStat(ITEM_SPIKED)) {
    weapon->addToCurSharp(-limbdam);
    weapon->addToMaxStructPoints(-1);
    weapon->addToStructPoints(-limbdam);
    if (!victim->isLimbFlags(limb, IMMUNE_BLEED) && !victim->isUndead()) {
      victim->rawBleed(limb, 250, SILENT_YES, CHECK_IMMUNITY_NO);
      act(
        "The wound begins to bleed as the spikes on $o shred flesh, but the "
        "weapon is damaged!",
        false, this, weapon, victim, TO_ROOM);
    } else {
      act(
        "The wound becomes jagged as the spikes on $o rip through flesh, but "
        "the weapon is damaged!",
        false, this, weapon, victim, TO_ROOM);
    }
    victim->hurtLimb(1, limb);
  }
  if (!victim->isLimbFlags(limb, PART_MISSING) &&
      ((victim->isLimbFlags(limb, PART_BLEEDING) ||
        victim->isLimbFlags(limb, PART_INFECTED) ||
        victim->isLimbFlags(limb, PART_BRUISED)))) {
    if (victim->getCurLimbHealth(limb) >= victim->getMaxLimbHealth(limb) / 2) {
      if (limb == WEAR_NECK || limb == WEAR_BODY || limb == WEAR_BACK ||
          limb == WEAR_WAIST) {
        return false;
      } else if (limb == WEAR_HEAD && !victim->hasDisease(DISEASE_EYEBALL)) {
        affectedData tAff;
        act("You glance $N's eyes with your $o, slicing them wide open!", false,
          this, 0, victim, TO_CHAR);
        act("$n glances your eyes with $s $o, slicing them wide open!", false,
          this, 0, victim, TO_VICT);
        act("$n glances $N's eyes with $s $o, slicing them wide open!", false,
          this, 0, victim, TO_NOTVICT);

        tAff.type = AFFECT_DISEASE;
        tAff.level = 0;
        tAff.duration = PERMANENT_DURATION;
        tAff.modifier = DISEASE_EYEBALL;
        tAff.location = APPLY_NONE;
        tAff.bitvector = AFF_BLIND;
        victim->affectTo(&tAff);
        victim->rawBlind(this->GetMaxLevel(), tAff.duration, SAVE_NO);
      } else {
        victim->makePartMissing(limb, false, this);
        act("You slice $N's %s right off!", false, this, weapon, victim,
          TO_CHAR);
        act("$n slices your %s right off!", false, this, 0, victim, TO_VICT);
        act("$n slices $N's %s right off!", false, this, 0, victim, TO_NOTVICT);
      }

      // Determine damage type
      spellNumT damageType = DAMAGE_NORMAL;

      if (weapon->isPierceWeapon())
        damageType = DAMAGE_IMPALE;
      // Reconcile damage
      if (reconcileDamage(victim, dam, damageType) == -1)
        return DELETE_VICT;
      return true;
    }
  }

  if (weapon->isPoisoned()) {
    weapon->applyPoison(victim);
    act("You poison $N with your stab!", false, this, NULL, victim, TO_CHAR);
    act("That bastard $n just poisoned you!", false, this, NULL, victim,
      TO_VICT);
    act("$N gets a pale look on their face, like they've been poisoned!", false,
      this, NULL, victim, TO_NOTVICT);
  } else if (!victim->isLimbFlags(limb, PART_INFECTED) &&
             victim->isLimbFlags(limb, PART_BLEEDING) && !::number(0, 4)) {
    victim->rawInfect(limb, 250, SILENT_YES, CHECK_IMMUNITY_YES);
    act("Your stab to $N's %s infects it!", false, this, NULL, victim, TO_CHAR);
    act("Your %s gets infected from $n's stab!", false, this, NULL, victim,
      TO_VICT);
    act("$N's %s gets infected from $n's stab!", false, this, NULL, victim,
      TO_NOTVICT);
  }
  spellNumT damageType = TYPE_PIERCE;

  if (reconcileDamage(victim, dam, damageType) == -1)
    return DELETE_VICT;

  return true;
}

int TBeing::stabFailure(TBeing* victim) {
  if (victim->getPosition() > POSITION_DEAD) {
    act("You miss your thrust into $N.", FALSE, this, 0, victim, TO_CHAR);
    act("$n misses $s thrust into $N.", FALSE, this, 0, victim, TO_NOTVICT);
    act("$n misses $s thrust into you.", FALSE, this, 0, victim, TO_VICT);
    this->reconcileDamage(victim, 0, SKILL_STABBING);
  }

  if (reconcileDamage(victim, 0, SKILL_STABBING) == -1)
    {return DELETE_VICT;}

  return true;
}
/*
int TBeing::doStab(const char* argument, TBeing* thief, TBeing* vict,
  TGenWeapon* weapon) {
int rc;
const int STAB_MOVE = 2;
int level;
TBeing* victim;

  if (checkBusy()) {
    return false;
  }

  // Ensure player even knows the skill before continuing
  if (!doesKnowSkill(SKILL_STABBING)) {
    sendTo(
      "You simply do not have the training necessary to stab someone
effectively.\n\r"); return false;
  }

  if (!(victim = vict)) {
    if (!(victim = get_char_room_vis(this, argument))) {
      if (!(victim = fight())) {
        sendTo("Hit whom?\n\r");
        return false;
      }
    }
  }
  if (!sameRoom(*victim)) {
    sendTo("That person isn't around.\n\r");
    return false;
  }

  auto* weapon = dynamic_cast<TGenWeapon*>(heldInPrimHand());

  // Ensure this isn't a peaceful room
  if (checkPeaceful("You feel too peaceful to contemplate violence.\n\r"))
    return false;

  // Make sure the player has enough vitality to use the skill
  if (getMove() < STAB_MOVE) {
    sendTo("You don't have the vitality to make the move!\n\r");
    return false;
  }

  // Prevent players from attacking immortals
  if (victim->isImmortal() || IS_SET(victim->specials.act, ACT_IMMORTAL)) {
    sendTo("Attacking an immortal would be a bad idea.\n\r");
    return false;
  }

  // Prevent players from attacking themselves
  if (victim == this) {
    sendTo("Do you REALLY want to kill yourself?...\n\r");
    return false;
  }

  // Avoid players attacking un-harmable victims
  if (noHarmCheck(victim))
    return false;

  // Limit players from using this while mounted
  if (riding) {
    sendTo("You can't perform that attack while mounted!\n\r");
    return false;
  }

  // Ensure the player has a weapon equipped
  if (!weapon) {
    sendTo(
      "You need to hold a weapon in your primary hand to do a stabbing.\n\r");
    return false;
  }
    if (!weapon->canStab()) {
      act("You can't use $o to stab.", false, thief, weapon, NULL, TO_CHAR);
      return FALSE;
    }

    if (thief->riding) {
      thief->sendTo("Not while mounted!\n\r");
      return FALSE;
    }

    if (dynamic_cast<TBeing*>(victim->riding)) {
      thief->sendTo("Not while that person is mounted!\n\r");
      return FALSE;
    }

    if (thief->noHarmCheck(victim))
      return FALSE;

    if (thief->getMove() < STAB_MOVE) {
      thief->sendTo("You are too tired to stab.\n\r");
      return FALSE;
    }

    thief->addToMove(-STAB_MOVE);

    thief->reconcileHurt(victim, 0.06);

    level = thief->getSkillLevel(SKILL_STABBING);
    int bKnown = thief->getSkillValue(SKILL_STABBING);
  int skillLevel = getSkillLevel(SKILL_STABBING);
  int skillValue = getSkillValue(SKILL_STABBING);
  int successfulHit = specialAttack(victim, SKILL_STABBING);
  int successfulSkill = bSuccess(skillValue, SKILL_STABBING);

    int i = thief->specialAttack(victim, SKILL_STABBING);

    // Success use case
if (!victim->awake() || (successfulHit && successfulSkill &&
                              successfulHit != GUARANTEED_FAILURE)) {
  rc = stabSuccess(thief,victim, weapon);

  // Fail use case
 } else {
    rc = stabFailure(thief,victim, weapon);
  }

  if (rc)
    addSkillLag(SKILL_STABBING, rc);

  if (IS_SET_DELETE(rc, DELETE_VICT)) {
    if (vict)
      return rc;
    delete victim;
    victim = nullptr;
    REM_DELETE(rc, DELETE_VICT);
  }

  if (IS_SET_DELETE(rc, DELETE_THIS)) {
    return DELETE_THIS;
  }

  return rc;
}
int TBeing::stabSuccess( TBeing* victim, TGenWeapon* weapon) {
  int skillLevel = getSkillLevel(SKILL_STABBING);
  int dam = getSkillDam(victim, SKILL_STABBING, skillLevel,
getAdvLearning(SKILL_STABBING)); auto* weapon =
dynamic_cast<TGenWeapon*>(heldInPrimHand()); wearSlotT limb =
victim->getPartHit(thief, FALSE); int limbdam = (weapon->getCurSharp() / 47) +
(this->GetMaxLevel() / 22); if (weapon->isObjStat(ITEM_SPIKED)()){ limbdam =+ 1;
  }
/// checking for the possibility of causing a bleed on victim
  bool LimbCanBleed = !victim->isLimbFlags(limb, PART_MISSING) &&
!victim->isUndead() && !victim->isLimbFlags(limb, IMMUNE_BLEED);  return
LimbCanBleed;
  }
    /// checking to see if weapon is sharp and if mob is tough
  bool LimbGetsHurt = !victim->isTough() &&
(weapon->getCurSharp()>=weapon->getMaxSharp()); return LimbGetsHurt;
  }
  /// checking limb health and bleeding and bruising
  bool LimbCanBeSevered = !victim->isLimbFlags(limb, PART_MISSING) &&
(victim->isLimbFlags(limb, PART_BLEEDING) || victim->isLimbFlags(limb,
PART_BRUISED)) && victim ->getCurHealt(limb) >= victim->(getMaxHealth(limb)/2));
return LimbCanBeSevered;
  }

  /// if an object is SPIKED, then it will potentially be damaged, if it's a
weapon, it will lose sharpness
  }
  if (weapon->isObjStat(ITEM_SPIKED)()) {
   dam = +2;
  }
  ////now we mess up the limb and cause some bleeding. if the limb is bleeding
  ///and half-wounded, cut it off!

///mob fails save and has bleedable limbs
if (limbsCanBleed(limb) && limbsCanGetHurt(limbs)) {
   int duration = (thief->getMaxLevel() * 3 + 200));
   victim->rawBleed(limb, duration, SILENT_YES, CHECK_IMMUNITY_NO);  }
   int rc = vict->hurtLimb(limbdam, limb);
   act("You puncture $N's %s with your $o, leaving a bloody gash!", false, this,
weapon, victim TO_CHAR); act("$n punctures $N's %s with their $o, leaving a
bloody gash!", false, this, weapon, victim TO_CHAR); act("$n punctures your $s
with their $o, leaving a bloody gash!!", false, this, weapon, victim TO_CHAR);

///mobs fails save and has no bleedable limbs
} else if (
 act("You puncture $N's %s with your $o, leaving a gaping hole!", false, this,
weapon, victim TO_CHAR); act("$n punctures $N's %s with their $o, leaving a
gaping hole!", false, this, weapon, victim TO_CHAR); act("$n punctures your $s
with their $o, leaving a gaping hole!!", false, this, weapon, victim TO_CHAR);
    if (weapon->isObjStat(ITEM_SPIKED)) {
     weapon->addToCurSharp(-limbdam);
     weapon->addToMaxStructPoints(-1);
     weapon->addToStructPoints(-limbdam);
     act("The wound becomes jagged as the spikes on $o rip through flesh, but
the weapon is damaged!", false, thief, weapon, TO_ROOM);
    }
///mob succeeds save
} else (
   act("You stab $N's %s with your $o, leaving a big wound!", false, this,
weapon, victim TO_CHAR); act("$n stabs $N's %s with their $o, leaving a big
wound!", false, this, weapon, victim TO_CHAR); act("$n stab your $s with their
$o, leaving a big wound!!", false, this, weapon, victim TO_CHAR);
}

    if (weapon->isObjStat(ITEM_SPIKED)() && && limbsCanBleed(limbs)) {
     victim->rawBleed(limb, 250 SILENT_YES, CHECK_IMMUNITY_NO);
     weapon->spikesBreak(victim, thief, weapon, limbdam);
     act("Blood pours as the spikes on $o rip through flesh, but the weapon is
damaged!", false, thief, weapon, TO_ROOM); } else if
(weapon->isObjStat(ITEM_SPIKED)() && (victim->isImmune(IMMUNE_BLEEDING) ||
victim->isUndead())){ weapon->addToCurSharp(-limbdam);
       weapon->addToMaxStructPoints(-1);
       weapon->addToStructPoints(-limbdam);

    }
}
if (weapon->isObjStat(ITEM_SPIKED)) {
     victim->rawBleed(limb, 250 SILENT_YES, CHECK_IMMUNITY_NO);
  if(limbsCanBleed(limb)) {
    spikesRip(victim, thief, weapon, limbdam);
  }
     act("Blood pours as the spikes on $o rip through flesh, but the weapon is
damaged!", false, thief, weapon, TO_ROOM);
    }

)

    if (!limb == WEAR_NECK || !limb == WEAR_BODY || !limb == WEAR_BACK || !limb
== WEAR_WAIST){ return false; } else if (limb == WEAR_HEAD &&
!tSucker->hasDisease(DISEASE_EYEBALL)) { affectedData tAff; act("You glance $N's
eyes with your $o, slicing them wide open!",false, this, 0, victim) act("$n
glances your eyes with $s $o, slicing them wide open!", false, this, 0, victim)
    act("$n glances $N's eyes with $s $o, slicing them wide open!", false, this,
0, victim)

    tAff.type = AFFECT_DISEASE;
    tAff.level = 0;
    tAff.duration = PERMANENT_DURATION;
    tAff.modifier = DISEASE_EYEBALL;
    tAff.location = APPLY_NONE;
    tAff.bitvector = AFF_BLIND;
    victim->affectTo(&tAff);
    victim->rawBlind(thief->GetMaxLevel(), tAff.duration, SAVE_NO);
    } else if (victim->isBruised(limb) : victim->isBleeding(limb)) :
victim->isUndead() : (victim->isImmune(IMMUNE_BLEEDING) &&
victim->isImmune{IMMUNE_SKIN_COND)) && victim ->getCurHealt(limb) >=
victim->(getMaxHealth(limb)/2){ victim->makePartMissing(limb, false, thief);
    act("You slice $N's %s right off!", false, thief, weaopn, victim, TO_CHAR);
    act("$n slices your %s right off!", false, this, 0, victim, TO_VICT);
    act("$n slices $N's %s right off!", false, this, 0, victim, TO_NOTVICT);
    } else {
    return false;
    }


              for (int tSwingIndex = 0; tSwingIndex < MAX_SWING_AFFECT;
                 tSwingIndex++) {
              int tDuration =
                (tThief->GetMaxLevel() * Pulse::UPDATES_PER_MUDHOUR);

      if (weapon->isPoisoned()) {
        weapon->applyPoison(victim);
        act("You poison $N with your stab!", false, thief, NULL, victim,
TO_CHAR); act("That bastard $n just poisoned you!", false, thief, NULL, victim,
TO_VICT); act("$N gets a pale look on their face, like they  poisoned!",false,
thief, NULL, victim, TO_NOTVICT); } else if (!victim->isLimbFlags(limb,
PART_INFECTED)) && !::number(0, 4)){ victim->rawInfect(limb, tDuration,
SILENT_YES, CHECK_IMMUNITY_YES)) { act("Your stab to $N's %s infects it!" false,
thief, NULL, victim, TO_CHAR); act ("Your %s gets infected from $n's stab!")
false, thief, NULL, victim, TO_VICT); act ("$N's %s gets infected from $n's
stab!") false, thief, NULL, victim, TO_NOTVICT);
      }



            if (dam == -1 ||
                thief->reconcileDamage(victim, dam, tDamageType) == -1)
              return DELETE_VICT;

            if (obj->checkSpec(victim, CMD_STAB, reinterpret_cast<char*>(limb),
                  thief) == DELETE_VICT)
              return DELETE_VICT;

            return TRUE;

            bool tKill = thief->willKill(victim, tDamage, tDamageType, false);
return tKill;
            }
            if (thief->willKill(victim, dam, SKILL_STABBING, false)) {
              act("Your hand is coated in ichor as you slit $N's guts!",
false,thief, weapon, victim, TO_CHAR, ANSI_RED); act( "Ichor spews from the
gaping stab wound in $N's lifeless body!", true, thief, weapon, victim,
TO_NOTVICT); if (limb == WEAR_NECK) { victim->makeBodyPart(WEAR_HEAD, victim);
                  victim->dropPool(50, LIQ_BLOOD);
                  act(" $N's head rolls to a stop!", false, victim, NULL,
victim, TO_ROOM);
              }
            }
int TBeing::stabFailure(TBeing* thief, TBeing* victim, TGenWeapon* weapon)
              act("You miss your thrust into $N.", FALSE, thief, obj, victim,
                TO_CHAR);
              act("$n misses $s thrust into $N.", FALSE, thief, obj, victim,
                TO_NOTVICT);
              act("$n misses $s thrust into you.", FALSE, thief, obj, victim,
                TO_VICT);
              thief->reconcileDamage(victim, 0, SKILL_STABBING);
            }
*/