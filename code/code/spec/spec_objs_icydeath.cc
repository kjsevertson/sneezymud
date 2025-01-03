#include <stdio.h>

#include "being.h"
#include "comm.h"
#include "extern.h"
#include "obj_base_weapon.h"

int icyDeath(TBeing* vict, cmdTypeT cmd, const char* arg, TObj* obj, TObj* thief) {

  TBeing* ch = genericWeaponProcCheck(vict, cmd, obj, 0);

  int level = ch->GetMaxLevel();

  if (!ch || !percentChance(25))
    return false;
  int dam = ::number(2 + level / 10, 10 + level / 10);
  const char* color = dam < 11 ? ANSI_CYAN : ANSI_BLUE;
  sstring msg = dam < 11 ? "freezes" : "sends a violent chill through";
  sstring toChar = format("$p becomes covered with ice and %s you.") % msg;
  sstring toRoom = format("$p becomes covered with ice and %s $n.") % msg;

  if (dam >= 11) {
    affectedData aff;
    aff.type = SPELL_FROST_BREATH;
    aff.level = level;
    aff.duration = level / 2 * Pulse::UPDATES_PER_MUDHOUR;
    aff.modifier = -(level / 5);
    aff.location = APPLY_IMMUNITY;
    aff.bitvector = IMMUNE_COLD;
    vict->affectTo(&aff);
  }

  act(toRoom, true, vict, obj, nullptr, TO_ROOM, color);
  act(toChar, false, vict, obj, nullptr, TO_CHAR, color);

  int rc = ch->reconcileDamage(vict, dam, DAMAGE_FROST);
  if (IS_SET_DELETE(rc, DELETE_VICT)) {
    return DELETE_VICT;
  }  


// Thief will only == nullptr when proc is called on the generic command proc
// check. Don't want proc to execute in that instance - should only happen in
// response to skill use. Otherwise it'll execute twice as often as it should.
if ((cmd != CMD_STAB && cmd != CMD_BACKSTAB && cmd != CMD_SLIT) ||
    !thief || !vict || !obj)
  return false;

auto* weapon = dynamic_cast<TBaseWeapon*>(obj);

// Double cast required because thief comes in having already been cast
// from TBeing to TThing to TObj, and casting directly from TObj to TBeing
// is not allowed (outside of reinterpret_cast).
auto* thiefAsTBeing = dynamic_cast<TBeing*>(dynamic_cast<TThing*>(thief));

// Verify stabber is wielding the object in their primary hand
if (!weapon || !thiefAsTBeing || !(thiefAsTBeing->heldInPrimHand() == weapon))
  return false;

wearSlotT limb = WEAR_NOWHERE;

// This reinterpret_cast is necessary because there's no clean way to pass the
// wearSlotT value of the stab location into the function, short of completely
// changing the function signature of every object spec proc function.
if (cmd == CMD_STAB)
  limb = static_cast<wearSlotT>(reinterpret_cast<uintptr_t>(arg));

limb = cmd == CMD_BACKSTAB ? WEAR_BACK
       : cmd == CMD_SLIT   ? WEAR_NECK
                               : limb;

if (limb == WEAR_NOWHERE || limb == HOLD_RIGHT || limb == HOLD_LEFT ||
    limb == MAX_WEAR)
  return false;

// 10% chance on stab, 50% on backstab/slit
if (cmd == CMD_STAB && ::number(0, 4))
  return false;

if (::number(0, 1))
  return false;

int damage = 0;
spellNumT damageType = DAMAGE_FROST;

if (cmd == CMD_STAB) {
  damage = level * 1.5;
} else if (cmd == CMD_BACKSTAB) {
  damage = level * 2.5;
  damageType = DAMAGE_FROST;
  act("<B>The weapon <W>freezes <B>as it pierces $N's back!<z>", false,
    thiefAsTBeing, weapon, vict, TO_CHAR);
  act("<B>The weapon <W>freezes <B>down $N's spine!<z>", false, thiefAsTBeing,
    weapon, vict, TO_ROOM);
} else {
  damage = level * 3;
  damageType = DAMAGE_FROST;
  act("<B>The weapon <W>freezes <B>through $N's throat!<z>", false,
    thiefAsTBeing, weapon, vict, TO_CHAR);
  act("<B>The weapon <W>freezes <B>through $N's throat!<z>", false,
    thiefAsTBeing, weapon, vict, TO_ROOM);
}
/// this portion of the proc embeds an icicle into the victim, if the wielder's
/// cold immunity is high enough, then adds a bleed effect if the victim is not
/// immune to bleeding

int rc = thiefAsTBeing->reconcileDamage(vict, damage, damageType);
if (IS_SET_DELETE(rc, DELETE_VICT))
  return DELETE_VICT;
if (ch->getImmunity(IMMUNE_COLD) < 85)
  return false;

int coldimmunity = ch->getImmunity(IMMUNE_COLD);
obj = read_object(13713, VIRTUAL);
dam = ::number(coldimmunity / 12, coldimmunity / 5);

sstring buf = format("<B>A jagged <W>icicle <B>from <1>$p<B>'s blade breaks off while "
    "embedded in <1>$n<k>'s <1>%s<k>.<1>") % vict->describeBodySlot(limb);
act(buf, false, vict, obj, nullptr, TO_ROOM);

vict->stickIn(obj, wearSlotT(limb));
if (vict->slotChance(limb) && !vict->isImmune(IMMUNE_BLEED, WEAR_BACK) &&
    !vict->isLimbFlags(limb, PART_BLEEDING) && !vict->isUndead()) {
  act("<r>Blood begins to pour from the wound!<z>", false, vict, nullptr,
    nullptr, TO_ROOM);
  vict->rawBleed(limb, level * 3 + 100, SILENT_YES, CHECK_IMMUNITY_NO);
}
return true;

TBeing* ch;
affectedData aff1, aff2;
/// this portion of the proc is a pulse that gives a level based immunity to
/// cold damage if cold immunity is 100, it then adds an AC bonus to the wearer
 if (cmd != CMD_GENERIC_PULSE || ::number(0, 49 || !obj))
     return false;

     ch = dynamic_cast<TBeing*>(obj->equippedBy);
  int level = ch->GetMaxLevel();

     // cold immunity apply
     aff1.type = SPELL_PROTECTION_FROM_WATER;
     aff1.duration = Pulse::UPDATES_PER_MUDHOUR * 3;
     aff1.location = APPLY_IMMUNITY;
     aff1.modifier = IMMUNE_COLD;
     aff1.modifier2 = ((level * 2) / 3);
     aff1.bitvector = 0;
     aff1.level = 40;

     colorAct(COLOR_SPELLS, "<B>The air around you begins to freeze.<1>",
     FALSE, ch, NULL, 0, TO_CHAR);
     colorAct(COLOR_SPELLS,
     "<B>The air around $n begins to freeze<1>",
     FALSE, ch, NULL, 0, TO_ROOM);

    if (ch->getImmunity(IMMUNE_COLD)>=100)
     return false;
     // ARMOR APPLY
     aff2.type = SPELL_PROTECTION_FROM_WATER;
     aff2.level = 30;
     aff2.duration = 3 * Pulse::UPDATES_PER_MUDHOUR;
     aff2.location = APPLY_ARMOR;
     aff2.modifier = -(25+(level/2));

     colorAct(COLOR_SPELLS,
     "<B>Frost covers your skin, protecting you in a layer of <W>ice.<z>",
     FALSE, ch, NULL, 0, TO_CHAR);

     colorAct(COLOR_SPELLS,
     "<B>Frost covers $n's skin, protecting $m in a layer of <W>ice.<1>",
     FALSE, ch, NULL, 0, TO_ROOM);

  ch->affectJoin(ch, &aff1, AVG_DUR_NO, AVG_EFF_YES);

 /// this portion of the proc is a say command, which utilizes all of the
 /// wielder's current mana it produces a cold blast attack, which scales in
 /// intensity to the mana total.
 if (!(ch = dynamic_cast<TBeing*>(obj->equippedBy)))
   return FALSE;

 if (cmd == CMD_SAY || cmd == CMD_SAY2) {
   sstring buf, buf2;
   TBeing* vict = NULL;
   buf = sstring(arg).word(0);
   buf2 = sstring(arg).word(1);

   if (buf == "winter" && buf2 == "cometh") {
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

     ch->addObjUsed(obj, Pulse::UPDATES_PER_MUDHOUR);

     act(
       "The point of $n's $o glows <b>a cold blue<1> as $e growls a <p>word "
       "of power<1>.",
       TRUE, ch, obj, NULL, TO_ROOM, NULL);
     act("$n steps back and points $p at $N!<1>", TRUE, ch, obj, vict, TO_NOTVICT,
       NULL);
     act(
       "<c>A spray of frost erupts from $n's <b>$o<c>, and covers $N in "
       "ice!<1>",
       TRUE, ch, obj, vict, TO_NOTVICT, NULL);
     act("$n steps back and points $p at you!  Uh oh!<1>", TRUE, ch, obj, vict,
       TO_VICT, NULL);
     act(
       "<c>A spray of frost erupts from $n's <b>$o<c>, and covers you in "
       "ice!<1>",
       TRUE, ch, obj, vict, TO_VICT, NULL);
     act(
       "The point of $o glows <b>a cold blue<1> as you growl the <B>words of "
       "power<1>'.",
       TRUE, ch, obj, NULL, TO_CHAR, NULL);
     act("You step back and point $p at $N!<1>", TRUE, ch, obj, vict, TO_CHAR,
       NULL);
     act("<c>A spray of frost erupts from <b>$o<c>, and covers $n in ice!<1>",
       TRUE, ch, obj, vict, TO_CHAR, NULL);

  int mana = ch->getMana();
     ch->addToMana(-mana);
     int dam = 0;
     dam = mana * plotValue(static_cast<double>(coldimmunity), 0.0, 100.0, 0.1, 1.18, 0.58, 1.0); 
     rc = ch->reconcileDamage(vict, dam, DAMAGE_FROST);
     if (IS_SET_DELETE(rc, DELETE_VICT))
       return DELETE_VICT;

     ///
     if (mana >= 300) {
       act(
         "<c>The air around <1>$n<c> seems to waver, then becomes <B>extremely "
         "cold<1><c>!<1>",
         TRUE, ch, obj, NULL, TO_ROOM, NULL);
       act("<c>A blast of frigid air radiates from <1>$n<c>!<1>", TRUE, ch, obj,
         NULL, TO_ROOM, NULL);
       act(
         "<c>The air around you seems to waver, then becomes <B>extremely "
         "cold<1><c>!<1>",
         TRUE, ch, obj, NULL, TO_CHAR, NULL);
       act("<c>A blast of frigid air radiates from you<c>!<1>", TRUE, ch, obj,
         NULL, TO_CHAR, NULL);

       if (vict->riding) {
         act("The blast of <c>fro<b>zen <c>air<1> knocks $N from $S mount!",
           TRUE, ch, obj, vict, TO_CHAR, NULL);
         act("The blast of <c>fro<b>zen <c>air<1> knocks $N from $S mount!",
           TRUE, ch, obj, vict, TO_NOTVICT, NULL);
         act(
           "<o>The blast of <c>fro<b>zen <c>air<1> knocks you from your "
           "mount!<1>",
           TRUE, ch, obj, vict, TO_VICT, NULL);
         vict->dismount(POSITION_RESTING);
       }
       act(
         "The blast of <c>fro<b>zen <c>air<1> from your $o slams $N into the "
         "$g, stunning $M!",
         TRUE, ch, obj, vict, TO_CHAR, NULL);
       act(
         "The blast of <c>fro<b>zen <c>air<1> from $n's $o slams $N into the "
         "$g, stunning $M!",
         TRUE, ch, obj, vict, TO_NOTVICT, NULL);
       act(
         "The blast of <c>fro<b>zen <c>air<1> from $n's $o slams you into the "
         "$g, stunning you!",
         TRUE, ch, obj, vict, TO_VICT, NULL);

       affectedData aff;
       aff.type = SKILL_DOORBASH;
       aff.duration = Pulse::TICK * 4;
       aff.bitvector = AFF_STUNNED;
       vict->affectTo(&aff, -1);
     }
     if (mana >= 450) {
       ;
       vict->cantHit += vict->loseRound(coldimmunity / 50);
       act(
             "Tendrils of the coldest cold settle within your body, stunning "
             "you!",
         FALSE, ch, 0, vict, TO_VICT);
       act(
             "A massive chill enters into $N, causing $S to slow tremendously "
             "for moment!",
         FALSE, ch, 0, vict, TO_NOTVICT);
     }
     if (mana > 600) {
       ;
       int limbdam = ::number(2, 10);

       act("The <W>frost<z> covers $N, freezing $S limbs!", FALSE, ch, 0,
         vict, TO_CHAR);
       act("The <W>frost<z> covers you, freezing your limbs!", FALSE, ch,
         0, vict, TO_VICT);
       act("The <W>frost<z> covers $N, freezing $S limbs!", FALSE, ch, 0,
         vict, TO_NOTVICT);

       int num = 0;
       wearSlotT slot;
       for (num = 0; num < limbdam; num++) {
         int num2 = 0;  // prevent endless loops
         for (slot = MIN_WEAR; slot < MAX_WEAR && num2 < 10; slot++) {
           num2++;
           if (!vict->hasPart(slot))
             continue;
           if (!vict->getCurLimbHealth(slot))
             continue;
           int rc = vict->hurtLimb(1, slot);
           if (IS_SET_DELETE(rc, DELETE_THIS))
             return DELETE_VICT;
         }
       }

       if (!ch || !percentChance(mana / 10)) {
         return false;
         int rc = ch->reconcileDamage(ch, dam, DAMAGE_TRAP_PIERCE);
         colorAct(COLOR_SPELLS,
           "<R>Deadly shards of ice tear through your flesh, impaling you from "
           "within.<z>",
           FALSE, ch, NULL, 0, TO_CHAR);

         colorAct(COLOR_SPELLS,
           "<R>Jagged shards of ice explode from within $n's body, impaling "
           "them "
           "$m in a bloody spray.<1>",
           FALSE, ch, NULL, 0, TO_ROOM);

         if (IS_SET_DELETE(rc, DELETE_VICT)) {
           vict->reformGroup();
           delete vict;
           vict = NULL;
         }
       }
       return TRUE;
     }
   }
 }
}
