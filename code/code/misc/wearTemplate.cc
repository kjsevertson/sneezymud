//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD++ - All rights reserved, SneezyMUD Coding Team
//
//      "wearTemplate.cc" - Blank prototypes for creating wearable objects
//
//////////////////////////////////////////////////////////////////////////

#include "wearTemplate.h"

#include "extern.h"
#include "obj.h"

TObj* makeBlankWearable(itemTypeT type, TemplateSlot slot) {
  int vnum = templateVnum(type, slot);
  if (!vnum)
    return nullptr;

  TObj* obj = read_object(vnum, VIRTUAL);
  if (!obj)
    return nullptr;

  // Own the flag word outright rather than inheriting the prototype's, then
  // string before anything writes a name or description -- swapToStrung()
  // deep copies the prototype's ex_description, which is otherwise shared.
  obj->setObjStat(0);
  obj->swapToStrung();

  return obj;
}
