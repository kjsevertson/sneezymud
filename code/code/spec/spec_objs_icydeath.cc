#include <stdio.h>

#include "being.h"
#include "comm.h"
#include "extern.h"
#include "obj_base_weapon.h"

int ColdHit(TBeing* ch, TBeing* vict, TObj* obj, int level) {
  TObj* icicle = read_object(31349, REAL);
  /*
  Damage is slightly higher than a standard proc and based on wielder level
  If damage is beyond a threshold, the proc gives an affect that reduces
  victim's immunity to cold. This is meant to be the basis of a compound tactic
  that capitalizes on a single high-risk/high-reward Cold damage spell-like
  attack Given the level dependence of the damage spread of the onHit proc, it
  will apply 11% of the time at level 10, and roughly 55% of the time at
  level 50. The second part of the onHit
  */
  int dam = ::number(2 + level / 10, 10 + level / 10);
  if (dam <= 11) {
    act("$p becomes covered with ice and freezes $n.", 0, vict, obj, 0, TO_ROOM,
      ANSI_CYAN);
    act("$p becomes covered with ice and freezes you.", 0, vict, obj, 0,
      TO_CHAR, ANSI_CYAN);
  } else {
    act("$p becomes covered with ice and sends a violent chill through $n.",
      false, vict, obj, nullptr, TO_ROOM, ANSI_BLUE);
    act("$p becomes covered with ice and sends a violent chill through you.",
      false, vict, obj, nullptr, TO_CHAR, ANSI_BLUE);

    affectedData aff;
    aff.type = SPELL_FROST_BREATH;
    aff.level = level;
    aff.duration = level / 2 * Pulse::UPDATES_PER_MUDHOUR;
    aff.modifier = -(level / 5);
    aff.location = APPLY_IMMUNITY;
    aff.bitvector = IMMUNE_COLD;
    vict->affectTo(&aff);

    if (dam >= 15) {
      wearSlotT limb = vict->getRandomPart(PART_MISSING);
      act(
        "<C>The air around $p becomes an<1> <W>arctic blizzard,<1> <C>which "
        "freezes $n to $s core!<1>",
        false, vict, obj, nullptr, TO_ROOM, ANSI_BLUE);
      act(
        "<C>The air around $p becomes an<1> <W>arctic blizzard,<1> <C>which "
        "freezes you to your core!<1>",
        false, vict, obj, nullptr, TO_CHAR, ANSI_BLUE);
      if (!vict->isImmune(IMMUNE_COLD, WEAR_BODY)) {
        vict->stickIn(icicle, limb);
        act(
          "A chunk of $N's blood is frozen solid, leaving behind a jagged "
          "<W>icicle<1>.",
          false, vict, obj, nullptr, TO_NOTVICT);
        act(
          "A chunk of your blood is frozen solid, leaving behind a jagged "
          "<W>icicle<1>.",
          false, vict, obj, nullptr, TO_VICT);
      }
    }
  }
  int rc = ch->reconcileDamage(vict, dam, DAMAGE_FROST);
  if (rc == -1)
    return DELETE_VICT;
  return TRUE;}

int ColdShroud(TBeing* ch, TObj* obj, int level) {
  affectedData aff1;
  affectedData aff2;

  ch = dynamic_cast<TBeing*>(obj->equippedBy);
  // This applies a level based immunity to cold, which tops out at 35%
  aff1.type = SPELL_PROTECTION_FROM_WATER;
  aff1.duration = Pulse::UPDATES_PER_MUDHOUR * 3;
  aff1.location = APPLY_IMMUNITY;
  aff1.modifier = IMMUNE_COLD;
  aff1.modifier2 = ((level * 7) / 10);
  aff1.bitvector = 0;
  aff1.level = 40;
  act("A frigid wind emerges from $p and envelopes $N.", false, ch, nullptr,
    nullptr, TO_ROOM, ANSI_BLUE);
  act("A frigid wind emerges from $p and envelopes you.", false, ch, obj,
    nullptr, TO_CHAR, ANSI_BLUE);
  if (ch->getImmunity(IMMUNE_COLD) >= 100) {
    // ARMOR APPLY
    // checks for the wielder's immunity to cold and if it is 100% it applies
    // a level based AC bonus
    aff2.type = SPELL_PROTECTION_FROM_WATER;
    aff2.level = 30;
    aff2.duration = 3 * Pulse::UPDATES_PER_MUDHOUR;
    aff2.location = APPLY_ARMOR;
    aff2.modifier = -(25 + (level / 2));
  }
  act("The air around $N coallesces into a layer of protective frost.", false,
    ch, nullptr, nullptr, TO_ROOM, ANSI_CYAN);
  act("The air around you coallesces into a layer of protective frost.", false,
    nullptr, nullptr, nullptr, TO_CHAR, ANSI_CYAN);

    return true;
}

int coldBlast(TBeing* ch, TBeing* vict, TObj* obj, int level,
  int coldimmunity) {
  TObj* icicle = read_object(31349, REAL);

  if (ch->checkObjUsed(obj)) {
    act("You cannot use $p's powers again this soon.", TRUE, ch, obj, NULL,
      TO_CHAR, NULL);
    return FALSE;
  }

  if (!(vict = ch->fight())) {
    act("You cannot use $p's powers unless you are fighting.", TRUE, ch, obj,
      NULL, TO_CHAR, NULL);
    return FALSE;
  }

  if (vict->isImmune(IMMUNE_COLD, WEAR_BODY)) {
    act("You cannot use $p's powers on someone who is immune to cold.", TRUE,
      ch, obj, NULL, TO_CHAR, NULL);
    return FALSE;
  }

  ch->addObjUsed(obj, 3 * Pulse::UPDATES_PER_MUDHOUR);
  int manaSpent = ch->getMana();
  ch->addToMana(-manaSpent);
  int dam = 0;
  dam = manaSpent * (coldimmunity / 80);
  dam = dam * (level / 50);
 
  act(
    "A storm blooms as you call out to the ancient souls of those taken "
    "by the brutal cold!",
    false, nullptr, nullptr, nullptr, TO_CHAR, ANSI_BLUE);
  act(
    "A storm blooms as $n calls out to the ancient souls of those taken "
    "by the brutal cold!",
    false, ch, nullptr, nullptr, TO_ROOM, ANSI_BLUE);

  act(
    "Howling winds erupt from$p, pummeling into $N's body with intense "
    "fury!",
    false, nullptr, obj, vict, TO_ROOM, ANSI_BLUE);
  act(
    "Howling winds erupt from $p, pummeling into your body with intense "
    "fury!",
    false, nullptr, obj, vict, TO_VICT, ANSI_BLUE);
  act(
    "Howling winds erupt from $p, pummeling into $N's body with intense "
    "fury!",
    false, nullptr, obj, vict, TO_CHAR, ANSI_BLUE);
  int rc = ch->reconcileDamage(vict, dam, DAMAGE_FROST);
    if (rc == -1)
     return DELETE_VICT;
    return TRUE;
  if (manaSpent >= 300) {
    affectedData aff;
    aff.type = SKILL_DOORBASH;
    aff.duration = Pulse::TICK * 4;
    aff.bitvector = AFF_STUNNED;
    vict->affectTo(&aff, -1);

    if (vict->riding) {
      vict->dismount(POSITION_STUNNED);
      return true;
    }

    if (manaSpent >= 400) {
      vict->cantHit += vict->loseRound(3 + (coldimmunity / 50));
      act("The storm rages around $N, freezing $M to the core!", false, nullptr,
        obj, vict, TO_ROOM, ANSI_BLUE_BOLD);
      act("The storm rages around you, freezing you to the core!", false,
        nullptr, obj, vict, TO_VICT, ANSI_BLUE_BOLD);
    }

    if (manaSpent >= 500) {
      int numLimbs = ::number(manaSpent / 100 - 2, manaSpent / 100 + 2);
      std::vector<wearSlotT> validLimbs;

      for (wearSlotT limb; limb < MAX_WEAR; limb++) {
        if (vict->hasPart(limb) && !vict->equipment[limb] &&
            !vict->getStuckIn(limb)) {
          validLimbs.push_back(limb);
        }
      }

      while (numLimbs > 0 && !validLimbs.empty()) {
        unsigned int index = ::number(0, validLimbs.size() - 1);
        wearSlotT limb = validLimbs[index];
        sstring buf = format(
                        "<B>The icy winds tear into $N, battering their %s "
                        "with<1> <W>shards of ice!<1>") %
                      vict->describeBodySlot(limb);
        act(buf, false, vict, nullptr, nullptr, TO_ROOM);
        buf = format(
                        "<B>The icy winds tear into you, battering your %s "
                        "with<1> <W>shards of ice!<1>") %
                      vict->describeBodySlot(limb);
        act(buf, false, vict, nullptr, nullptr, TO_VICT);

        vict->hurtLimb(::number(manaSpent / 100, manaSpent / 100 + 10), limb);

        if (manaSpent >= 600) {
          vict->stickIn(icicle, limb, SILENT_YES);
          sstring buf =
            format("<W>An icicle<1> <C>embeds itself into $N's %s!<1>") %
            vict->describeBodySlot(limb);
          act(buf, false, vict, nullptr, nullptr, TO_ROOM);
          buf =
            format("<W>An icicle<1> <C>embeds itself into your %s!<1>") %
            vict->describeBodySlot(limb);
          act(buf, false, vict, nullptr, nullptr, TO_VICT);
        }

        --numLimbs;

        if (index != validLimbs.size() - 1) {
          std::swap(validLimbs[index], validLimbs.back());
        }
        validLimbs.pop_back();
      }
    }
  }
  if (!ch || !percentChance(manaSpent / 60)) {
    return false;
    int rc = ch->reconcileDamage(ch, dam, DAMAGE_TRAP_PIERCE);
    act(
      "Deadly shards of ice tear through your flesh, impaling you "
      "from "
      "within.",
      FALSE, ch, nullptr, nullptr, TO_CHAR, ANSI_BLUE_BOLD);

    act(
      "Jagged shards of ice explode from within $n's body, impaling "
      "them  in a bloody spray.",
      FALSE, ch, nullptr, nullptr, TO_ROOM, ANSI_BLUE_BOLD);

    if (IS_SET_DELETE(rc, DELETE_VICT)) {
      vict->reformGroup();
      delete vict;
      vict = NULL;
    }
  }
  return true;
}


int coldThief(TBeing* ch, TBeing* vict, TObj* obj, int level) {
  TObj* icicle = read_object(31349, REAL);
  int rc = false;
  wearSlotT limb = WEAR_BACK;
  if (vict->isImmune(IMMUNE_COLD, WEAR_BODY) && ::number(0, 1))
    return false;

  int dam = 0;

  dam = level * 3;
  act("<W>$p turns bright white and freezes down $N's spine!<z>", false,
    nullptr, obj, vict, TO_CHAR);
  act("<W>$p turns bright white and freezes down $N's spine!<z>", false,
    nullptr, obj, vict, TO_ROOM);
  act("<W>$p turns bright white and freezes down your spine!<z>", false,
    nullptr, obj, vict, TO_VICT);

  rc = ch->reconcileDamage(vict, dam, DAMAGE_FROST);
  if (rc == -1)
    return DELETE_VICT;
  return TRUE;

  if (vict->slotChance(limb) && !vict->equipment[limb] &&
      !vict->getStuckIn(limb)) {
    act(
      "$p freezes as it pierces $N's %s, leaving behind a jagged "
      "icicle!<z>",
      false, vict, obj, nullptr, TO_ROOM, ANSI_BLUE_BOLD);
    act("$p freezes as it pierces your %s, leaving behind a jagged icicle!",
      false, vict, obj, nullptr, TO_VICT, ANSI_BLUE_BOLD);
    act(
      "Your $p freezes as it pierces $N's %s, leaving behind a jagged "
      "icicle!",
      false, vict, obj, nullptr, TO_CHAR, ANSI_BLUE_BOLD);
    vict->stickIn(icicle, limb);
  }
  return true;
}
int icyDeath(TBeing* vict, cmdTypeT cmd, const char* arg, TObj* obj, TObj*) {
  TBeing* ch = dynamic_cast<TBeing*>(obj->equippedBy);
  if (!ch)
    return FALSE;  
  int rc;
  int level = ch->GetMaxLevel();
  int coldimmunity = ch->getImmunity(IMMUNE_COLD);
  rc = false;
  int dam = 0;
  if (cmd == CMD_OBJ_HIT && vict && percentChance(20)) {
    ColdHit(ch, vict, obj, level);
    rc = ch->reconcileDamage(vict, dam, DAMAGE_FROST);
    if (IS_SET_DELETE(rc, DELETE_VICT)) {
      vict->reformGroup();
      delete vict;
      vict = NULL;
    }
  }

  if (cmd == CMD_GENERIC_PULSE && percentChance(5)) {
    ColdShroud(ch, obj, level);
    return true;
  }

  if (cmd == (CMD_SAY) || cmd == (CMD_SAY2)) {
    sstring buf, buf2;
    TBeing* vict = NULL;
    buf = sstring(arg).word(0);
    buf2 = sstring(arg).word(1);
    int dam = 0;

    if (buf == "winter" && buf2 == "cometh") {
      coldBlast(ch, vict, obj, level, coldimmunity);
      rc = ch->reconcileDamage(vict, dam, DAMAGE_FROST);
      if (IS_SET_DELETE(rc, DELETE_VICT)) {
        return DELETE_VICT;
      }
      return false;
    }
  }

  if (cmd == CMD_BACKSTAB) {
    coldThief(ch, vict, obj, level);
  }
  return false;
}
