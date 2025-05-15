#include "enum.h"
#include "handler.h"
#include "being.h"
#include "combat.h"
#include "limbs.h"
#include "obj_base_weapon.h"
#include "obj_general_weapon.h"
#include "monster.h"

int TBeing::doHamstring(const char* argument, TBeing* vict) {
  int rc;
  TBeing* victim;
  const int HAMSTRING_MOVE = 3;

  if (checkBusy()) {
    return false;
  }

  // Ensure player even knows the skill before continuing
  if (!doesKnowSkill(SKILL_HAMSTRING)) {
    sendTo(
      "You wouldn't even know where to begin in executing that maneuver.\n\r");
    return false;
  }

  // Prevent using hamstring while tanking
  if (isTanking()) {
    sendTo("You can't position yourself for a hamstring while tanking!\n\r");
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

  // Check for weapon in primary hand first
  auto* primaryWeapon = dynamic_cast<TGenWeapon*>(heldInPrimHand());
  
  // If no suitable weapon in primary hand, check secondary hand for thieves with high dual wield
  auto* secondaryWeapon = dynamic_cast<TGenWeapon*>(heldInSecHand());
  bool useSecondaryWeapon = false;
  
  // Allow thieves with 75+ dual wield to use offhand weapon for hamstring
  if (!primaryWeapon && secondaryWeapon && 
      hasClass(CLASS_THIEF) && 
      doesKnowSkill(SKILL_DUAL_WIELD) && 
      getSkillValue(SKILL_DUAL_WIELD) >= 75 &&
      secondaryWeapon->canHamstring()) {
    useSecondaryWeapon = true;
  } else {
    // Use primary weapon
    useSecondaryWeapon = false;
  }
  
  // Get the weapon we'll be using
  auto* weapon = useSecondaryWeapon ? secondaryWeapon : primaryWeapon;

  // Ensure this isn't a peaceful room
  if (checkPeaceful("You feel too peaceful to contemplate violence.\n\r"))
    return false;

  // Make sure the player has enough vitality to use the skill
  if (getMove() < HAMSTRING_MOVE) {
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

  if (victim->isFlying() && !this->isFlying()) {
    sendTo("You can't hamstring a flying target unless you are also flying!\n\r");
    return false;
  }

  // Ensure the player has a suitable weapon equipped
  if (!weapon) {
    sendTo(
      "You need to hold a suitable weapon to make this a success.\n\r");
    return false;
  }

  if (!weapon->canHamstring()) {
    if (useSecondaryWeapon) {
      act("You can't use a $o in your off-hand to hamstring.", false, this, weapon, NULL, TO_CHAR);
    } else {
      act("You can't use a $o to hamstring.", false, this, weapon, NULL, TO_CHAR);
    }
    return FALSE;
  }

  // Only consume vitality for mortals
  if (!(isImmortal() || IS_SET(specials.act, ACT_IMMORTAL)))
    addToMove(-HAMSTRING_MOVE);

  int modifier = 0;

  // Add stealth modifiers similar to backstab
  modifier -= noise(this) / 20;
  modifier += visibility() / 15;

  if (isAffected(AFF_SNEAK))
    modifier += 5;
  if (isAffected(AFF_HIDE))
    modifier += 5;
  if (victim->fight() && !fight())
    modifier += 5;
  
  // Apply penalty for using off-hand weapon
  if (useSecondaryWeapon) {
    // Calculate penalty based on dual wield skill (less penalty with higher skill)
    int dualWieldSkill = getSkillValue(SKILL_DUAL_WIELD);
    int penalty = 10 - ((dualWieldSkill - 75) / 2.5); // Penalty ranges from 10 to 0
    penalty = std::max(0, penalty); // Ensure penalty doesn't go negative
    modifier -= penalty;
    
    act("You attempt to hamstring with your off-hand weapon.", FALSE, this, 0, victim, TO_CHAR);
  }
  
  if (makesNoise() && victim->awake()) {
    modifier -= 10;

    act(
      "$n's armor makes too much noise, and $N is alerted to $n's hamstring attempt.",
      FALSE, this, 0, victim, TO_NOTVICT);
    act("You make too much noise, and $N hears you coming.", FALSE, this, 0,
      victim, TO_CHAR);
    act(
      "You hear $n's armor, and quickly attempt to dodge $s attempt to hamstring you.",
      FALSE, this, 0, victim, TO_VICT);
  }

  // Add awareness check like backstab
  if (victim->awake() && victim->canSee(this) && !victim->isPc() &&
      dynamic_cast<TMonster*>(victim)->isSusp()) {
    modifier -= 10;

    act("$E is able to see you and notices you coming at the last moment.",
      FALSE, this, 0, victim, TO_CHAR);
    act("You sense $m coming as $n attempts to hamstring you.", FALSE, this, 0,
      victim, TO_VICT);
  }

  // Add height difference bonus to attack roll
  // Shorter attackers get bonus against taller victims
  if (this->getHeight() < victim->getHeight()) {
    // Calculate a ratio that gives 20% bonus when attacker is 31 and victim is 90
    double heightRatio = (victim->getHeight() - this->getHeight()) / 59.0; // 90-31 = 59
    heightRatio = std::min(1.0, heightRatio); // Cap at 1.0 for maximum bonus
    
    // Per combat.cc balance notes, 16.667 points of mod is equivalent to one level's worth
    // 20% bonus would be approximately 3.33 levels worth of bonus
    int heightBonus = static_cast<int>(heightRatio * 3.33 * 16.667);
    modifier += heightBonus;
    
    if (heightBonus > 0) {
      act("Your smaller stature gives you an advantage in targeting $N's legs.",
        FALSE, this, 0, victim, TO_CHAR);
    }
  }

  int skillValue = getSkillValue(SKILL_HAMSTRING);
  // Pass the modifier to specialAttack with custom stats
  int successfulHit = specialAttack(victim, SKILL_HAMSTRING, modifier,
                                   STAT_DEX, STAT_AGI, 
                                   STAT_AGI, STAT_PER, true);
  int successfulSkill = bSuccess(skillValue, SKILL_HAMSTRING);

  // Success use case
  if (!victim->awake() || (successfulHit && successfulSkill &&
                            successfulHit != GUARANTEED_FAILURE)) {
    rc = hamstringSuccess(victim);
  }
  // Fail use case
  else {
    rc = hamstringFail(victim);
  }

  if (rc)
    addSkillLag(SKILL_HAMSTRING, rc);

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

int TBeing::hamstringSuccess(TBeing* victim) {
  int skillLevel = getSkillLevel(SKILL_HAMSTRING);
  int dam = getSkillDam(victim, SKILL_HAMSTRING, skillLevel, getAdvLearning(SKILL_HAMSTRING));
  int limbDam = dam/2;
  
  // Check primary hand first, then secondary if primary doesn't have a suitable weapon
  auto* primaryWeapon = dynamic_cast<TGenWeapon*>(heldInPrimHand());
  auto* secondaryWeapon = dynamic_cast<TGenWeapon*>(heldInSecHand());
  
  // Determine which weapon was used
  auto* weapon = primaryWeapon;
  if (!primaryWeapon || !primaryWeapon->canHamstring()) {
    if (secondaryWeapon && secondaryWeapon->canHamstring() && 
        hasClass(CLASS_THIEF) && 
        doesKnowSkill(SKILL_DUAL_WIELD) && 
        getSkillValue(SKILL_DUAL_WIELD) >= 75) {
      weapon = secondaryWeapon;
    }
  }
  
  int rc;
  sstring buf;
  sstring weaponStr = weapon->getName();
  wearSlotT hitLimb = WEAR_LEG_R;
  
  if (percentChance(50) || !victim->hasPart(WEAR_LEG_R)) {
    hitLimb = WEAR_LEG_L;
  }

  buf = format("Your %s rips through $N's tendon on $S lower leg!") % weaponStr;
  act(buf, FALSE, this, 0, victim, TO_CHAR, ANSI_ORANGE);
  buf = format("$n's %s rips through the tendon in your lower leg.") % weaponStr;
  act(buf, FALSE, this, 0, victim, TO_VICT, ANSI_RED);
  buf = format("$n's %s rips into $N, tearing the tendon in $S lower leg.") % weaponStr;
  act(buf, FALSE, this, 0, victim, TO_NOTVICT, ANSI_BLUE);
  
  rc = reconcileDamage(victim, dam, SKILL_HAMSTRING);
  if (IS_SET_DELETE(rc, DELETE_VICT))
    return DELETE_VICT;
  
  for (int i = 1; i < 5; i++) {
    if (victim->equipment[hitLimb])
      victim->damageItem(this, hitLimb, SKILL_HAMSTRING, weapon, false);
  }

  if (victim->isBruised(hitLimb) || victim->isBleeding(hitLimb)) {
    limbDam = limbDam * 1.5;
  }

  if (weapon->isObjStat(ITEM_SPIKED)) {
    limbDam = limbDam * 1.25;
    spikesHit(victim, this, weapon, hitLimb);
  }

  if (this->getPosition() >= POSITION_STANDING || this->isFlying()) {
    if (victim->getPosition() > POSITION_SITTING) {
      // Calculate height difference - shorter attackers get bonus against taller victims
      // For a 31 tall attacker vs 90 tall victim, we want a multiplier of 1.33
      double heightRatio = 0.0;
      if (this->getHeight() < victim->getHeight()) {
        // Calculate a ratio that gives 0.33 bonus when attacker is 31 and victim is 90
        heightRatio = (victim->getHeight() - this->getHeight()) / 59.0; // 90-31 = 59
        heightRatio = std::min(1.0, heightRatio); // Cap at 1.0 for maximum bonus
      }
      double multiplier = 1.0 + (heightRatio * 0.33); // 0.33 is the maximum bonus
      limbDam = static_cast<int>(limbDam * multiplier);
    } else {
      limbDam = static_cast<int>(limbDam * 1.5);
    }
  }
  victim->setMove(victim->getMove() / 2);
  rc = victim->hurtLimb(limbDam, hitLimb);
  if (IS_SET_DELETE(rc, DELETE_THIS)) {
    return DELETE_VICT;
  }

  if (!victim->isTough()) {
    if ((weapon->getCurSharp() > ::number(0,99)) && victim->getCurLimbHealth(hitLimb) < victim->getMaxLimbHealth(hitLimb) / 2){
      victim->makePartMissing(hitLimb, false, this);
      sstring limbName = victim->describeBodySlot(hitLimb);
      sstring weaponName = weapon->getName();

      act(format("Your %s completely severs $N's %s!") % weaponName % limbName,
          FALSE, this, nullptr, victim, TO_CHAR, ANSI_RED_BOLD);
      act(format("$n's %s completely severs your %s!") % weaponName % limbName,
          FALSE, this, nullptr, victim, TO_VICT, ANSI_RED_BOLD);
      act(format("$n's %s completely severs $N's %s!") % weaponName % limbName,
          FALSE, this, nullptr, victim, TO_NOTVICT, ANSI_RED_BOLD);
      hitLimb = victim->findNextAttachedBodyPart(hitLimb);
    }
    rawBleed(hitLimb, 250, SILENT_YES, CHECK_IMMUNITY_NO);
    act("Blood begins to flow from the wound!", FALSE, victim, 0, 0, TO_ROOM,
      ANSI_RED_BOLD);
    act("Blood begins to flow from the wound!", FALSE, victim, 0, 0, TO_CHAR,
      ANSI_RED_BOLD);
  }

  rc = reconcileDamage(victim, dam, SKILL_HAMSTRING);
  if (rc == -1) {
    return DELETE_VICT;
  }

  return true;
}

int TBeing::hamstringFail(TBeing* victim) {
  // Check primary hand first, then secondary if primary doesn't have a suitable weapon
  auto* primaryWeapon = dynamic_cast<TGenWeapon*>(heldInPrimHand());
  auto* secondaryWeapon = dynamic_cast<TGenWeapon*>(heldInSecHand());
  
  // Determine which weapon was used
  auto* weapon = primaryWeapon;
  if (!primaryWeapon || !primaryWeapon->canHamstring()) {
    if (secondaryWeapon && secondaryWeapon->canHamstring() && 
        hasClass(CLASS_THIEF) && 
        doesKnowSkill(SKILL_DUAL_WIELD) && 
        getSkillValue(SKILL_DUAL_WIELD) >= 75) {
      weapon = secondaryWeapon;
    }
  }

  if (victim->getPosition() > POSITION_DEAD) {
    act("$n's attempt to hamstring $N with $s $p fails awkwardly.",
      FALSE, this, weapon, victim, TO_NOTVICT);
    act("Your attempt to hamstring $N with your $p fails awkwardly.",
      FALSE, this, weapon, victim, TO_CHAR);
    act("$n attempts to hamstring you with $s $p but fails!", 
      FALSE, this, weapon, victim, TO_VICT);
  }

  if (reconcileDamage(victim, 0, SKILL_HAMSTRING) == -1)
    return DELETE_VICT;

  return true;
}

