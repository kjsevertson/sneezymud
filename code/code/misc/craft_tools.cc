//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//      "craft_tools.cc" - The tools a piece of craft work needs to hand
//
//////////////////////////////////////////////////////////////////////////

#include "craft_tools.h"

#include "comm.h"
#include "room.h"
#include "extern.h"
#include "obj_tool.h"
#include "being.h"

TTool* findHeldTool(const TBeing* ch, int toolType, bool primary) {
  if (!toolType || !ch)
    return nullptr;

  TTool* tt = nullptr;

  if ((primary || ch->isAmbidextrous()) && ch->heldInPrimHand() &&
      (tt = dynamic_cast<TTool*>(ch->heldInPrimHand())) &&
      tt->getToolType() == toolType)
    return tt;

  if ((!primary || ch->isAmbidextrous()) && ch->heldInSecHand() &&
      (tt = dynamic_cast<TTool*>(ch->heldInSecHand())) &&
      tt->getToolType() == toolType)
    return tt;

  return nullptr;
}

TTool* findRoomTool(const TBeing* ch, int toolType) {
  if (!toolType || !ch)
    return nullptr;

  TRoom* rp = real_roomp(ch->in_room);
  if (!rp)
    return nullptr;

  for (StuffIter it = rp->stuff.begin(); it != rp->stuff.end(); ++it) {
    TTool* tt = dynamic_cast<TTool*>(*it);
    if (tt && tt->getToolType() == toolType)
      return tt;
  }

  return nullptr;
}

const char* toolTypeName(int toolType) {
  switch (toolType) {
    case TOOL_WHETSTONE:
      return "whetstone";
    case TOOL_ANVIL:
      return "anvil";
    case TOOL_FORGE:
      return "forge";
    case TOOL_HAMMER:
      return "hammer";
    case TOOL_LOCKPICK:
      return "lockpick";
    case TOOL_NEEDLE:
      return "needle";
    case TOOL_THREAD:
      return "spool of thread";
    case TOOL_POISON:
      return "poison";
    case TOOL_GARROTTE:
      return "garrotte";
    case TOOL_FILE:
      return "file";
    case TOOL_BOWSTRING:
      return "bowstring";
    case TOOL_SKIN_KNIFE:
      return "skinning knife";
    case TOOL_HOLYWATER:
      return "vial of holy water";
    case TOOL_FLINTSTEEL:
      return "flint and steel";
    case TOOL_TOTEM:
      return "totem";
    case TOOL_FISHINGBAIT:
      return "fishing bait";
    case TOOL_BUTCHER_KNIFE:
      return "butchering knife";
    case TOOL_SEED:
      return "seed";
    case TOOL_TONGS:
      return "pair of tongs";
    case TOOL_OPERATING_TABLE:
      return "operating table";
    case TOOL_SCALPEL:
      return "scalpel";
    case TOOL_FORCEPS:
      return "pair of forceps";
    case TOOL_LADEL:
      return "ladel";
    case TOOL_SOIL:
      return "measure of soil";
    case TOOL_PLANT_OIL:
      return "flask of plant oil";
    case TOOL_CHALK:
      return "chalk";
    case TOOL_RUNES:
      return "set of runes";
    case TOOL_ENERGY:
      return "source of energy";
    case TOOL_PENTAGRAM:
      return "pentagram";
    case TOOL_CHISEL:
      return "chisel";
    case TOOL_SILICA:
      return "measure of silica";
    case TOOL_WORKBENCH:
      return "workbench";
    case TOOL_LOUPE:
      return "loupe";
    case TOOL_PLIERS:
      return "pair of pliers";
    case TOOL_PUNCH:
      return "punch";
    case TOOL_CORDING:
      return "length of cording";
    case TOOL_TAPE:
      return "roll of tape";
    case TOOL_CANDLE:
      return "candle";
    case TOOL_GLUE:
      return "pot of glue";
    case TOOL_ALTAR:
      return "altar";
    case TOOL_BRUSH:
      return "brush";
    case TOOL_ASTRAL_RESIN:
      return "astral resin";
    case TOOL_BLACK_POWDER:
      return "black powder";
    default:
      return "tool";
  }
}

bool hasCraftTools(TBeing* ch, const CraftTools& tools) {
  if (!ch)
    return false;

  // Hands first, then the room: a player who is holding nothing wants to hear
  // about the hammer before being sent looking for a forge.
  if (tools.primary && !findHeldTool(ch, tools.primary, true)) {
    ch->sendTo(format("You need to be holding a %s to do that.\n\r") %
               toolTypeName(tools.primary));
    return false;
  }

  if (tools.secondary && !findHeldTool(ch, tools.secondary, false)) {
    ch->sendTo(format("You need a %s in your other hand to do that.\n\r") %
               toolTypeName(tools.secondary));
    return false;
  }

  if (tools.room1 && !findRoomTool(ch, tools.room1)) {
    ch->sendTo(
      format("You need a %s here.\n\r") % toolTypeName(tools.room1));
    return false;
  }

  if (tools.room2 && !findRoomTool(ch, tools.room2)) {
    ch->sendTo(
      format("You need a %s here.\n\r") % toolTypeName(tools.room2));
    return false;
  }

  return true;
}
