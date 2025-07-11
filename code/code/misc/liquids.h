#pragma once

#include "obj.h"

// this contains data for a single liquid type
// meant to be used as an element in a list of types
class liqEntry {
  public:
    int drunk;
    int hunger;
    int thirst;
    bool potion;
    bool poison;
    const char* color;
    const char* name;
    int price;

    liqEntry(int, int, int, bool, bool, const char*, const char*, int);

  private:
    liqEntry();  // deny usage in this format
};

// this is a container class for a list of liquid types
class liqInfoT_pimpl;
class liqInfoT {
    liqInfoT_pimpl* pimpl;

  public:
    const liqEntry* operator[](const liqTypeT) const;

    liqInfoT();
    ~liqInfoT();
};

// Checks if enough water is available from environment or containers
// If water is found in a container, sets container pointer if not null
// Returns true if enough water is available
bool checkWaterUsage(TBeing* ch, int reqUnits, TBaseCup** container);
