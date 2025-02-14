
/*int disease_corrode(TBeing* victim, int message, affectedData* af) {
  char buf[256];
  wearSlotT slot = wearSlotT(af->level);
  int level, dam, rc;

  if (slot < MIN_WEAR || slot >= MAX_WEAR) {
    vlogf(LOG_BUG, format("disease_corrode called with bad slot: %i") % slot);
    return FALSE;
  }
  if (victim->isPc() && !victim->desc)
    return FALSE;

  switch (message) {
    case DISEASE_BEGUN:
      victim->addToLimbFlags(slot, PART_CORRODE);
      break;
    case DISEASE_PULSE:
      // check to see if somehow the corroded bit got taken off (via spell)
      if (!victim->hasPart(slot) || !victim->isLimbFlags(slot, PART_CORRODE)) {
        af->duration = 0;
        break;
      }

      }
      if (!number(0, 10)) {
        victim->sendTo(
          format(
            "Smoke rises from your %s as the <g>acid<1> corrodes you.\n\r") %
          victim->describeBodySlot(slot));
        sprintf(buf,
          "$n's %s releases a plume of smoke as the <g>acid<1> burns them.",
          victim->describeBodySlot(slot).c_str());
        act(buf, TRUE, victim, NULL, NULL, TO_ROOM);
        rc = victim->hurtLimb(1, slot);
        if (IS_SET_DELETE(rc, DELETE_THIS))
          return DELETE_THIS;
        dam = ::number(3, 6);
        if (victim->reconcileDamage(victim, dam, ACID_DAMAGE) == -1)
          return DELETE_THIS;

        if (dynamic_cast<TMonster*>(victim) && !victim->isPc())
          (dynamic_cast<TMonster*>(victim))->UA((dam * 4));
      }
      victim->bodySpread(500, af);
      break;
    case DISEASE_DONE:
      victim->remLimbFlags(slot, PART_CORRODE);
      if (victim->getPosition() > POSITION_DEAD && victim->hasPart(slot)) {
        victim->sendTo(format("The <g>acid<1> on your %s dissolves and stops smoking!\n\r") %
                       victim->describeBodySlot(slot));
        sprintf(buf, "The <g>acid<1> on $n's %s dissolves and stops smoking!",
          victim->describeBodySlot(slot).c_str());
        act(buf, TRUE, victim, NULL, NULL, TO_ROOM);
      }

      break;
    default:
      break;
  }
  return FALSE;
}*/
