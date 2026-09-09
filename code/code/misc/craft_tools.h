//////////////////////////////////////////////////////////////////////////
//
// SneezyMUD - All rights reserved, SneezyMUD Coding Team
//
//      "craft_tools.h" - The tools a piece of craft work needs to hand
//
//////////////////////////////////////////////////////////////////////////

#pragma once

class TBeing;
class TTool;

// Up to two tools held and up to two standing in the room. Zero means the work
// does not ask for that one.
struct CraftTools {
    int primary = 0;
    int secondary = 0;
    int room1 = 0;
    int room2 = 0;
};

// The tool of this type the character is holding, or null. `primary` chooses
// which hand is searched; an ambidextrous character may use either.
[[nodiscard]] TTool* findHeldTool(const TBeing* ch, int toolType, bool primary);

// The tool of this type standing in the character's room, or null.
[[nodiscard]] TTool* findRoomTool(const TBeing* ch, int toolType);

// A plain name for a tool type, for messages: "hammer", "operating table".
[[nodiscard]] const char* toolTypeName(int toolType);

// True when the character has everything the work asks for. Otherwise names
// the first thing missing and returns false, so a command can bail on the one
// call.
bool hasCraftTools(TBeing* ch, const CraftTools& tools);
