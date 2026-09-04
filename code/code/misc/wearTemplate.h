//////////////////////////////////////////////////////////////////////////
//
//      SneezyMUD++ - All rights reserved, SneezyMUD Coding Team
//
//      "wearTemplate.h" - Blank prototypes for creating wearable objects
//
//////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>

#include "low.h"
#include "obj.h"

class TObj;

// The twelve wearable slots the template prototypes cover, in the order the
// vnum blocks are laid out. The enumerator's value is its offset within a
// block, so this order is load-bearing -- see templateVnum() below.
enum class TemplateSlot {
  Head,
  Neck,
  Body,
  Back,
  Arm,
  Wrist,
  Hand,
  Waist,
  Leg,
  Foot,
  Shield,
  Finger,
  COUNT
};

inline constexpr int TEMPLATE_SLOT_COUNT =
  static_cast<int>(TemplateSlot::COUNT);

struct TemplateSlotInfo {
  TemplateSlot slot;
  unsigned int wearFlag;
};

inline constexpr std::array<TemplateSlotInfo, TEMPLATE_SLOT_COUNT>
  templateSlots = {{
      {TemplateSlot::Head, ITEM_WEAR_HEAD},
      {TemplateSlot::Neck, ITEM_WEAR_NECK},
      {TemplateSlot::Body, ITEM_WEAR_BODY},
      {TemplateSlot::Back, ITEM_WEAR_BACK},
      {TemplateSlot::Arm, ITEM_WEAR_ARMS},
      {TemplateSlot::Wrist, ITEM_WEAR_WRISTS},
      {TemplateSlot::Hand, ITEM_WEAR_HANDS},
      {TemplateSlot::Waist, ITEM_WEAR_WAIST},
      {TemplateSlot::Leg, ITEM_WEAR_LEGS},
      {TemplateSlot::Foot, ITEM_WEAR_FEET},
      {TemplateSlot::Shield, ITEM_WEAR_HOLD},
      {TemplateSlot::Finger, ITEM_WEAR_FINGERS},
  }};

// Vnum of the blank prototype for one wearable type and slot, or 0 for a type
// with no template block. The three blocks are contiguous and run head..finger,
// so the slot doubles as the offset from the block's first vnum.
[[nodiscard]] constexpr int templateVnum(itemTypeT type, TemplateSlot slot) {
  if (slot == TemplateSlot::COUNT)
    return 0;

  switch (type) {
    case ITEM_WORN:
      return Obj::TEMPLATE_CLOTHING_HEAD + static_cast<int>(slot);
    case ITEM_ARMOR:
      return Obj::TEMPLATE_ARMOR_HEAD + static_cast<int>(slot);
    case ITEM_JEWELRY:
      return Obj::TEMPLATE_JEWELRY_HEAD + static_cast<int>(slot);
    default:
      return 0;
  }
}

// The blocks really are contiguous and one slot-count apart. If someone
// renumbers them in low.h, this stops compiling instead of quietly handing
// back the wrong prototype.
static_assert(Obj::TEMPLATE_ARMOR_HEAD ==
              Obj::TEMPLATE_CLOTHING_HEAD + TEMPLATE_SLOT_COUNT);
static_assert(Obj::TEMPLATE_JEWELRY_HEAD ==
              Obj::TEMPLATE_ARMOR_HEAD + TEMPLATE_SLOT_COUNT);
static_assert(templateVnum(ITEM_WORN, TemplateSlot::Finger) ==
              Obj::TEMPLATE_CLOTHING_FINGER);
static_assert(templateVnum(ITEM_JEWELRY, TemplateSlot::Finger) ==
              Obj::TEMPLATE_JEWELRY_FINGER);

// Load a blank wearable of this type and slot, ready to be stamped with
// material, size, quality, affects and name.
//
// Objects that can change type or slot at runtime must be built this way
// rather than bare-constructed: rent does not persist an item's C++ class or
// wear_flags, it rebuilds them from the prototype at the item's vnum. A
// bare-constructed item has no vnum to come back to.
//
// The returned object is strung and owns its own flag word, which is cleared
// -- ITEM_NORENT and any anti-class flags are the caller's policy. Returns
// nullptr if no template exists for the type and slot.
[[nodiscard]] TObj* makeBlankWearable(itemTypeT type, TemplateSlot slot);
