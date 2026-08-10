---
primary_symbols: [runResetCmdM, doNewbieEqLoad, loadSetEquipment]
keywords: [bulk loot, mob equipment, race size, class gear, zone boot, volume modifier]
---

# Bulk Loot System (Design Document)

Global system for loading randomized gear onto mobs at zone boot, independent of
builder-specified zonefile commands. Augments existing E/G/Z/Y equipment loading.

## Goals

- Give every zone-booted mob a chance to spawn with randomized gear
- Gear selection based on **mob class** (warrior, mage, thief, etc.)
- Item volume adjusted to fit **mob race** using size modifiers
- Stacks on top of builder-specified equipment — does not replace it
- Not zone or mob specific — applies globally

## Race Size Modifiers

Derived from newbie starting gear body slot volumes (`code/code/misc/newbie.cc`).
Human (soft leather body, vnum 1002, volume 11000) is the baseline (1.000).

The oedit documentation defines the sizing system:
- Each wear slot has a 100% base volume (body=11000, leg=6000, waist=4000, etc.)
- An item's %size = volume / base_volume
- A character's %height = height / 70 (70 inches = average human male)
- Item fits if: %size * 0.85 < %height < %size * 1.15
- To create a correctly sized item: volume = (height / 70) * base_volume

### Base Volumes Per Slot (100% Size)

| Slot   | Volume |
|--------|--------|
| head   |   2500 |
| body   |  11000 |
| arm    |   2000 |
| wrist  |    400 |
| hand   |    800 |
| waist  |   4000 |
| back   |   2500 |
| leg    |   6000 |
| foot   |   1600 |

Paired items (e.g. pants) double the base before calculating %size.

### Race Modifiers (Body Slot Ratio to Human)

Derived from starting gear body slot volumes in the database:

| Race   | Body Vnum | Body Volume | Modifier |
|--------|-----------|-------------|----------|
| Hobbit |       982 |        5170 |    0.470 |
| Gnome  |       962 |        5720 |    0.520 |
| Dwarf  |       992 |        6930 |    0.630 |
| Fish   |     44774 |        6930 |    0.630 |
| Elven  |       952 |        8030 |    0.730 |
| Bully  |      4327 |        8030 |    0.730 |
| Human  |      1002 |       11000 |    1.000 |
| Bird   |     44834 |       11000 |    1.000 |
| Ogre   |       972 |       15070 |    1.370 |
| Troll  |     30933 |       15070 |    1.370 |

### Six Distinct Size Tiers

| Tier | Modifier | Races          |
|------|----------|----------------|
| 1    |    0.470 | Hobbit         |
| 2    |    0.520 | Gnome          |
| 3    |    0.630 | Dwarf, Fish    |
| 4    |    0.730 | Elven, Bully   |
| 5    |    1.000 | Human, Bird    |
| 6    |    1.370 | Ogre, Troll    |

### Volume Formula

To produce a correctly sized item for a given race and slot:

```
raceVolume = baseVolume * raceModifier
```

Where `baseVolume` is the 100% size for the slot (from the table above) and
`raceModifier` is from the race table.

## Hook Point

Bulk loot loads from two sites in `code/code/sys/db.cc`, both acting on a mob
whose own zonefile commands have already finished:

- `runResetCmdM()` generates on the *previous* mob before it starts a new one.
- `zoneData::resetZone()` generates on the final mob after the command loop ends.

Waiting until after the mob's own `E`/`G`/`Z`/`Y` commands is deliberate. By
then the builder's equipment is worn, so `bulkLoadOut()` can skip any slot that
is already filled and only add to what the builder left empty. Hooking at the
end of the `M` command that creates the mob would run before that equipment
exists, and bulk loot would compete with it instead of filling around it.

At this point the mob is fully constructed with:
- Race and class set
- Placed in room
- Wealth created
- Builder equipment already worn — occupied slots are skipped

Neither site runs during a load-potential scan (`resetFlagFindLoadPotential`).
That pass only tallies what a zone could produce; generating there would create
real items and buy real commodities off real shops at boot.

### Zone Boot Flow (Reference)

1. `bootZones()` → `bootOneZone()` → `zoneData::bootZone()` — parse zonefiles
2. `zoneData::renumCmd()` — convert vnums to real indices
3. `zoneData::resetZone()` — iterate `cmd_table`, dispatch commands
4. `runResetCmdM()` — first generates bulk loot on the *previous* mob ← **our
   hook**, then loads this mob into the room
5. Conditional E/G/Z/Y commands — load builder-specified equipment onto mob
6. Next `M` command, or the end of the loop for the last mob — bulk loot fills
   whichever slots step 5 left empty

Key files:
- `code/code/sys/db.cc` — zone reset logic, all runResetCmd* functions
- `code/code/sys/db.h` — resetCom, zoneData structs
- `code/code/misc/newbie.cc` — newbie gear arrays (size reference data)
- `lib/objdata/suitsets` — named suit set definitions

## Load Mechanics

On zone reset, each humanoid mob rolls 3% independently per equipment slot. Each slot that hits generates one piece using the mob's race, class, and level. This works the same way suitset loading does. Generated items load equipped on the mob and are lootable on death.

## Item Identity

Each generated item is defined by four variables:

1. **Racial size** — from the mob's race (determines volume and name prefix)
2. **Class type** — from the mob's class (determines base object name and armor tier)
3. **Stat type** — weighted random roll (determines material and ANSI color)
4. **Quality** — from the mob's level (determines name modifier, stat bonus, and skill/resource bonus)

Items are created by cloning a prototype, calling `swapToStrung()`, and setting the composed name/short_desc. Shops group items by vnum + short_desc, so distinct names list and sell as separate entries. The `rent_strung` table overrides the prototype's name/short_desc per instance.

## Racial Size Names

| Tier | Modifier | Races        | Name Prefix |
|------|----------|--------------|-------------|
| 1    | 0.470    | Hobbit       | tiny        |
| 2    | 0.520    | Gnome        | small       |
| 3    | 0.630    | Dwarf, Fish  | stout       |
| 4    | 0.730    | Elven, Bully | slim        |
| 5    | 1.000    | Human, Bird  | (default)   |
| 6    | 1.370    | Ogre, Troll  | large       |

"default" is omitted from the item name.

## Armor Tiers and Anti-Flags

The mob's class determines armor tier, which sets the material table and anti-flags.

| Tier     | Material Table | Generated By         | Anti-flags                                          |
|----------|---------------|----------------------|-----------------------------------------------------|
| Heavy    | Armor         | Warrior, Deikhan     | anti-cleric, anti-thief, anti-mage, anti-shaman, anti-monk |
| Medium   | Armor         | Cleric               | anti-thief, anti-mage, anti-shaman, anti-monk       |
| Light    | Clothing      | Thief, Mage          | anti-shaman, anti-monk                              |
| Clothing | Clothing      | Shaman, Monk         | (none)                                              |

AC scales by tier and mob level: heavy = 6/7 × level, medium = 5/7, light = 4/7, clothing = 3/7.

## Quality Tiers

Quality names differ for armor and clothing. "default" is omitted from the name.

| Level | Armor      | Clothing  | Stat Bonus | Skill Bonus | Resource Bonus |
|-------|------------|-----------|------------|-------------|----------------|
| 1-19  | flimsy     | cheap     | +1         | +5%         | —              |
| 20-29 | dented     | patched   | +2         | +5%         | —              |
| 30-39 | (default)  | (default) | +3         | —           | +3             |
| 40-49 | honed      | well-made | +4         | —           | +5             |
| 50-59 | superior   | excellent | +5         | —           | +7             |
| 60+   | masterwork | superb    | +6         | —           | +9             |

Low-tier items (levels 1-29) get a skill bonus. Mid-tier and above (30+) get a resource bonus instead. Stat bonuses apply at all tiers.

## Stat Selection (Weighted Random)

Each piece gets one stat. Primary stats for the mob's class have 3× weight; the remaining 9 stats have 1× weight. This gives each primary ~16.7% chance and each secondary ~5.6%.

### Class Primary Stats

| Class   | Primary Stats   |
|---------|----------------|
| Warrior | STR, BRW, AGI  |
| Deikhan | BRW, CON, WIS  |
| Cleric  | WIS, CON, PER  |
| Thief   | DEX, FOC, SPE  |
| Mage    | INT, FOC, CHA  |
| Shaman  | INT, FOC, CHA  |
| Monk    | DEX, SPE, FOC  |

## Stat → Material

The chosen stat determines the item's material, which appears in its name. The material table depends on the armor tier (armor materials for heavy/medium, clothing materials for light/clothing).

### Armor Materials

| Stat         | Material  |
|--------------|-----------|
| Strength     | steel     |
| Constitution | copper    |
| Brawn        | iron      |
| Dexterity    | mithril   |
| Agility      | electrum  |
| Speed        | titanium  |
| Wisdom       | platinum  |
| Intelligence | brass     |
| Focus        | silver    |
| Perception   | tin       |
| Karma        | aluminum  |
| Charisma     | gold      |

### Clothing Materials

| Stat         | Material         |
|--------------|------------------|
| Strength     | fur              |
| Constitution | wool             |
| Brawn        | horsehair        |
| Dexterity    | silk             |
| Agility      | cloth            |
| Speed        | rubber           |
| Wisdom       | hair             |
| Intelligence | feathered        |
| Focus        | hemp             |
| Perception   | dogfur           |
| Karma        | rabbitfur        |
| Charisma     | catfur           |

## Stat → ANSI Color

Each item's name is displayed in a color matching its stat.

| Stat         | Color         |
|--------------|---------------|
| Strength     | red           |
| Constitution | green         |
| Brawn        | orange        |
| Dexterity    | gray          |
| Agility      | bright yellow |
| Speed        | bright blue   |
| Wisdom       | blue          |
| Intelligence | cyan          |
| Focus        | bright cyan   |
| Perception   | bright green  |
| Karma        | bright purple |
| Charisma     | yellow        |

## Skill Bonuses (Levels 1-29 Only)

One skill per item, chosen with the same 3× class-weighting as stats.

| Class   | Skill Pool                                                        |
|---------|-------------------------------------------------------------------|
| Warrior | bash, slam, berserk, rescue                                      |
| Deikhan | charge, bash-deikhan, rescue, chivalry                           |
| Cleric  | heal-serious, heal-critical, flamestrike, penance, rain brimstone, devotion |
| Thief   | sneak, backstab, stab, track                                     |
| Mage    | mana, meditate, hands of flame, granite fists, flaming sword, wizardry |
| Shaman  | sacrifice, life leech, distort, squish, ritualism                |
| Monk    | yoginsa, kick, chop, springleap                                  |

## Resource Bonuses (Level 30+ Only)

One resource per item, chosen with 3× class-weighting. Four resources are possible for any class: HP, mana, movement, CBS.

| Class   | Weighted Resources |
|---------|--------------------|
| Warrior | HP, movement       |
| Deikhan | HP                 |
| Cleric  | HP                 |
| Thief   | CBS, movement      |
| Mage    | mana               |
| Shaman  | HP, CBS            |
| Monk    | movement, HP       |

## Class → Base Object Name by Slot

| Slot   | Warrior     | Deikhan     | Cleric      | Thief      | Mage      | Shaman    | Monk       |
|--------|-------------|-------------|-------------|------------|-----------|-----------|------------|
| Head   | spangenhelm | great helm  | helmet      | hood       | skullcap  | headdress | cowl       |
| Neck   | gorget      | throatguard | collar      | cravat     | scarf     | choker    | kerchief   |
| Body   | brigandine  | cuirass     | breastplate | vest       | robe      | tunic     | frock      |
| Back   | backplate   | coat        | mantle      | cloak      | cape      | shawl     | poncho     |
| Arm    | pauldron    | cannon      | brassard    | armband    | sleeve    | armlet    | wrap       |
| Wrist  | vambrace    | bracer      | maniple     | cuff       | bracelet  | bangle    | wristband  |
| Hand   | cestus      | gauntlet    | mitten      | half-glove | glove     | hand-wrap | fistguard  |
| Waist  | fauld       | girdle      | skirt       | belt       | cord      | loincloth | sash       |
| Leg    | cuisse      | greave      | shinguard   | legging    | trousers  | leg-wrap  | pants      |
| Foot   | sabaton     | jackboot    | boot        | shoe       | slipper   | sandal    | sock       |
| Finger | ring        | ring        | ring        | ring       | ring      | ring      | ring       |

Hold slots (weapons/shields) use a separate system.

## Name Construction

Format: `a [size] [quality] [material] [base name]`

"default" values for size and quality are omitted. Examples:

- Level 45 elf cleric wrist: `<blue>a slim honed platinum maniple</blue>` (+4 WIS, +5 HP)
- Level 10 dwarf warrior body: `<red>a stout flimsy steel brigandine</red>` (+1 STR, +5% bash)
- Level 55 human mage head: `<cyan>an excellent feathered skullcap</cyan>` (+5 INT, +7 mana)
- Level 35 ogre thief leg: `<gray>a large silk legging</gray>` (+3 DEX, +3 CBS)

Max visible name length: ~40 characters.

## Shields

Shields follow the same stat/class/quality rules as armor slots. Differences:

- No race size in name or keywords (like rings)
- 1.5× stat and resource bonuses (like rings)
- Full AC and struct points (unlike rings which get halved)
- Object type follows class armor tier: TArmor for heavy/medium, TWorn for light/clothing
- All classes generate shields
- Fixed base volume of 4500 (not race-scaled)

## Weapons (WIP)

### Weapon Properties

Weapons are one-size-fits-all — no race scaling. Each weapon name has a fixed volume and sharpness. Only weapons that appear in the class weapon pools below are listed here.

**Volume** is derived from existing DB examples. Where DB data was absent or clearly wrong, values are inferred from physically similar weapons.

**Sharpness** (`maxSharp`, 0-100) represents edge/face quality — how effective the weapon's striking surface is at what it does. A well-crafted mace with proper flanging scores high, same as a well-honed blade. A crude stick scores low. This isn't a sharp-vs-blunt axis; it's weapon quality. `curSharp` starts equal to `maxSharp` and degrades with use. Affects armor damage on hit, wound severity, and weapon pricing.

- 20-35: crude, unrefined (plain sticks, improvised weapons)
- 40-55: functional but simple (utility tools, basic metal heads, simple points)
- 55-70: proper combat weapon (standard swords, good hammers, war-purpose heads)
- 70-85: excellent weapon (fine blades, purpose-built war implements)
- 85-100: exceptional (katana, stiletto, precision instruments)

#### One-Handed Pierce

| Weapon | Volume | Sharp | Notes |
|--------|--------|-------|-------|
| dagger | 450 | 70 | DB vol cluster 300-600 |
| dirk | 500 | 70 | DB vol cluster 450-550 |
| epee | 3000 | 85 | DB vol 2200-4000; precision thrusting blade |
| gladius | 1000 | 65 | DB vol; broad workhorse stabbing sword |
| harpoon | 4000 | 50 | DB vol cluster; barbed point, thrown |
| katar | 800 | 85 | DB vol; focused punch blade |
| knife | 600 | 65 | DB vol cluster 400-800 |
| kris | 700 | 80 | DB vol 300-1300; wavy blade, well-crafted |
| misericorde | 450 | 90 | ~ dagger; purpose-built armor gap piercer |
| pick | 1200 | 55 | DB vol mid cluster; mining point |
| pickaxe | 1200 | 50 | ~ pick; utility tool |
| poniard | 1200 | 75 | DB vol; long thrusting dagger |
| rapier | 1500 | 85 | DB vol cluster 800-1750; precision weapon |
| sai | 550 | 55 | DB vol 500-600; trapping prongs with pointed tip |
| shortsword | 1000 | 65 | DB vol 800-1300 |
| spear | 2500 | 55 | DB vol cluster 1500-3000; simple point on shaft |
| stiletto | 650 | 95 | DB vol; pure piercing instrument |
| tanto | 1600 | 85 | DB vol; Japanese short blade |

#### Two-Handed Pierce

| Weapon | Volume | Sharp | Notes |
|--------|--------|-------|-------|
| estoc | 8000 | 85 | ~ 2h thrusting sword; stiff precision point |
| falx | 7000 | 70 | ~ 2h curved war blade |
| lance | 4000 | 50 | DB vol cluster; broad point, relies on momentum |
| pike | 2000 | 50 | DB vol (vnum 14408); long simple spike |
| ranseur | 7000 | 60 | DB vol; pronged pole weapon |
| trident | 3000 | 60 | DB vol 2000-4000; three functional prongs |
| warpick | 3000 | 65 | ~ 2h pick; purpose-built armor piercer |

#### One-Handed Slash

| Weapon | Volume | Sharp | Notes |
|--------|--------|-------|-------|
| axe | 3000 | 60 | DB vol cluster 1500-3100; wedge edge |
| battleaxe | 3000 | 65 | ~ axe (DB vol 433 is wrong); wider combat edge |
| broadsword | 8000 | 65 | DB vol 7500-8000; wide blade |
| cleaver | 7000 | 60 | DB vol 5300-9000; heavy chopping edge |
| cutlass | 2200 | 70 | DB vol cluster (3 at 2200); curved naval sword |
| falchion | 7000 | 70 | DB vol 6000-8000; heavy curved blade |
| hatchet | 1500 | 55 | DB vol 1000-2500; small utility axe |
| katana | 8000 | 95 | DB vol (4/5 at 8000); legendary edge |
| khopesh | 3500 | 65 | DB vol; sickle-sword |
| kukri | 800 | 75 | ~ large knife; recurved chopping blade |
| longsword | 7000 | 70 | DB vol 5000-8000 |
| machete | 800 | 60 | ~ large knife (DB vol 200 is wrong); bush knife |
| saber | 2500 | 75 | DB vol mid-cluster; cavalry blade |
| scimitar | 7000 | 75 | DB vol cluster 7000-8900; curved slashing blade |
| sickle | 1500 | 65 | ~ hatchet (DB vol 6600 is wrong); curved farm blade |
| sword | 6000 | 65 | DB vol "a long sword" cluster |
| tachi | 1500 | 90 | DB vol; long Japanese blade |
| tomahawk | 1500 | 55 | ~ hatchet; throwing axe |
| wakizashi | 800 | 90 | DB vol 433-1000; companion to katana |

#### Two-Handed Slash

| Weapon | Volume | Sharp | Notes |
|--------|--------|-------|-------|
| claymore | 8000 | 60 | DB vol (3/4 at 8000); heavy highland sword, mass over edge |
| fauchard | 7000 | 65 | ~ 2h polearm (DB vol 250 is wrong); curved pole blade |
| flamberge | 8000 | 70 | ~ claymore (DB vol 300 is wrong); wavy serrated blade |
| greataxe | 5000 | 60 | DB vol 3000-6000; heavy axe head |
| halberd | 3400 | 60 | DB vol 2450-4400; axe + spike on pole |
| naginata | 2500 | 80 | DB vol; Japanese pole blade |
| scythe | 3000 | 70 | DB vol 2200-4000 cluster; long curved blade |
| shamshir | 8000 | 80 | ~ 2h sword; Persian curved blade |
| tulwar | 8000 | 75 | ~ 2h sword; Indian curved blade |
| waraxe | 5000 | 60 | ~ greataxe (DB vol 1000 is wrong) |
| warblade | 8000 | 70 | DB vol |
| zanbatou | 9000 | 60 | DB vol; oversized horse-cutter, mass over edge |

#### One-Handed Blunt

| Weapon | Volume | Sharp | Notes |
|--------|--------|-------|-------|
| baton | 330 | 25 | DB vol; smooth rod |
| cane | 2500 | 25 | DB vol (excl "cane knife"); walking stick |
| club | 3000 | 35 | DB vol cluster; heavy wood |
| cudgel | 1000 | 35 | DB vol cluster; short heavy stick |
| flail | 2400 | 70 | DB vol mid-cluster; spiked ball on chain |
| hammer | 2000 | 55 | DB vol "plain hammer" cluster; proper metal head |
| mace | 3000 | 65 | DB vol cluster 3000-3500; flanged head, concentrates force |
| mallet | 3000 | 40 | DB vol cluster; flat-faced, not combat-optimized |
| morningstar | 2400 | 75 | ~ flail; spiked ball, devastating contact surface |
| scepter | 2500 | 45 | DB vol mid-range; metal-headed but ornamental |
| truncheon | 330 | 30 | DB vol; smooth baton |
| warhammer | 3200 | 70 | DB vol cluster; purpose-built striking face + beak |

#### Two-Handed Blunt

| Weapon | Volume | Sharp | Notes |
|--------|--------|-------|-------|
| greathammer | 5000 | 60 | ~ between hammer and maul; large concentrated head |
| kanabo | 4000 | 65 | ~ 2h war club; studded iron, purpose-built |
| martel | 4000 | 70 | ~ 2h hammer (DB vol 250 is wrong); hammer + pick head |
| mattock | 3000 | 60 | ~ maul; pick/adze hybrid |
| maul | 3000 | 50 | DB vol 900-5300 cluster; heavy head, relies on mass |
| quarterstaff | 4000 | 25 | DB vol 4000-4400 very consistent; smooth wood |
| sledgehammer | 5000 | 45 | ~ greathammer (DB vol 40 is wrong); flat heavy face |
| spade | 4000 | 50 | ~ staff; sharpened edge on blade |
| staff | 4000 | 20 | DB vol strong cluster; smooth wood |
| war club | 3500 | 45 | ~ large club; possibly studded |

### Damage Type Categories

Weapons use up to 2 damage types at 70/30 frequency split. Each type belongs to a proficiency category. A weapon is classified as its primary category if ≥2/3 of its frequency is in that category (checked by `isBluntWeapon()`, `isSlashWeapon()`, `isPierceWeapon()`).

**Blunt** (3 types): smash, bludgeon, strike

**Slash** (3 types): slash, cleave, slice

**Pierce** (3 types): stab, thrust, spear

Excluded types: hit (generic fallback), cannon, beak, shoot, bear_claw, shred, claw, bite, sting, whip.

### Weapon Profiles

162 total profiles per category: 27 per category × 2 (one-handed + two-handed) × 3 categories.

Each category has 27 profiles:
- 3 pure (100% one type)
- 6 within-category (70/30 split, both types from same category)
- 18 cross-category (70/30 split, secondary type from a different category)

Every profile has both a one-handed and two-handed variant, each with a unique weapon name. Two-handed weapons occupy both hold slots.

### Weapon Name Pool

Names are categorized by primary proficiency. Only weapons that appear in the class weapon pools are listed. 54 needed per category (27 one-handed + 27 two-handed) for full profile coverage.

**PIERCE** (25 available, need 54)
dagger, dirk, epee, gladius, harpoon, katar, knife, kris, misericorde, pick, pickaxe, poniard, rapier, sai, shortsword, spear, stiletto, tanto, estoc, falx, lance, pike, ranseur, trident, warpick

**SLASH** (31 available, need 54)
axe, battleaxe, broadsword, cleaver, cutlass, falchion, hatchet, katana, khopesh, kukri, longsword, machete, saber, scimitar, sickle, sword, tachi, tomahawk, wakizashi, claymore, fauchard, flamberge, greataxe, halberd, naginata, scythe, shamshir, tulwar, waraxe, warblade, zanbatou

**BLUNT** (22 available, need 54)
baton, cane, club, cudgel, flail, hammer, mace, mallet, morningstar, scepter, truncheon, warhammer, greathammer, kanabo, martel, mattock, maul, quarterstaff, sledgehammer, spade, staff, war club

### Assignment Status

All class-pool weapons have been assigned to profiles. Current counts:

| Category | 1h | 2h | Total | Target (54) |
|----------|----|----|-------|-------------|
| Pierce   | 18 |  7 |    25 |          -29 |
| Slash    | 19 | 12 |    31 |          -23 |
| Blunt    | 12 | 10 |    22 |          -32 |

Shortfalls only matter if we want exactly 27 profiles per handedness per category — more names can be invented later to fill gaps.

Note: Some weapons moved between pools during assignment (tachi from pierce→slash, falx from slash→pierce, fauchard from pierce→slash). The worksheet is authoritative; the name pools above are the original brainstorm lists.

## Open Design Questions

- **Item templates:** Clone from a pool of generic prototypes, or generate purely from code?
- **Exclusions:** Skip shopkeepers, quest NPCs, mobs with full gear from zonefile?

## Weapon Name Assignment Worksheet

Add a tag after each name: p/s/b for one-handed pure, ps/pb/sp/sb/bp/bs for one-handed cross, 2p/2s/2b for two-handed pure, 2ps/2pb/etc for two-handed cross. First letter = primary (70%), second = secondary (30%).

### Pierce (1h)
dagger - p
dirk - p
epee - p
gladius - ps
harpoon - p
katar - p
knife - ps
kris - ps
misericorde - p
pick - pb
pickaxe - pb
poniard - p
rapier - p
sai - pb
shortsword - ps
spear - p
stiletto - p
tanto - ps

### Pierce (2h)
estoc - 2p
falx - 2ps
lance - 2p
pike - 2p
ranseur - 2p
trident - 2p
warpick - 2pb

### Slash (1h)
axe - s
battleaxe - sb
broadsword - s
cleaver - s
cutlass - sp
falchion - sp
hatchet - sb
katana - s
khopesh - sp
kukri - s
longsword - s
machete - s
saber - s
scimitar - s
sickle - sp
sword - s
tachi - s
tomahawk - sb
wakizashi - sp

### Slash (2h)
claymore - 2s
fauchard - 2sp
flamberge - 2s
greataxe - 2s
halberd - 2sp
naginata - 2sp
scythe - 2s
shamshir - 2s
tulwar - 2s
waraxe - 2sb
warblade - 2sb
zanbatou - 2s

### Blunt (1h)
baton - b
cane - b
club - b
cudgel - b
flail - bp
hammer - b
mace - b
mallet - b
morningstar - bp
scepter - b
truncheon - b
warhammer - b

### Blunt (2h)
greathammer - 2b
kanabo - 2bp
martel - 2b
mattock - 2bp
maul - 2b
quarterstaff - 2b
sledgehammer - 2b
spade - 2bs
staff - 2b
war club - 2bs

### Class Weapon Pools

Universal weapons load on any mob regardless of class:

| Weapon | Damage |
|--------|--------|
| cane | 100% strike |
| club | 100% smash |
| hammer | 100% bludgeon |
| knife | 70% stab / 30% slice |
| spear | 100% spear |
| staff | 100% strike |
| sword | 70% slash / 30% thrust |

### Warrior

| Weapon | Damage |
|--------|--------|
| baton | 100% strike |
| battleaxe | 70% cleave / 30% strike |
| broadsword | 70% slash / 30% thrust |
| claymore | 70% cleave / 30% thrust |
| cleaver | 100% cleave |
| cutlass | 70% slash / 30% stab |
| falchion | 70% cleave / 30% slice |
| flail | 70% bludgeon / 30% strike |
| gladius | 70% thrust / 30% slash |
| greataxe | 100% cleave |
| halberd | 70% cleave / 30% thrust |
| harpoon | 100% spear |
| hatchet | 70% cleave / 30% slash |
| katana | 70% slice / 30% thrust |
| lance | 100% thrust |
| longsword | 70% slash / 30% thrust |
| mace | 100% bludgeon |
| mattock | 70% bludgeon / 30% stab |
| maul | 100% smash |
| morningstar | 70% smash / 30% stab |
| naginata | 70% slice / 30% spear |
| pickaxe | 70% stab / 30% strike |
| pike | 100% spear |
| shamshir | 70% slice / 30% slash |
| tomahawk | 70% slash / 30% strike |
| trident | 70% spear / 30% slash |
| tulwar | 70% slash / 30% slice |
| war club | 70% smash / 30% slash |
| waraxe | 70% cleave / 30% bludgeon |
| warblade | 70% slash / 30% smash |
| warhammer | 70% bludgeon / 30% thrust |
| warpick | 70% thrust / 30% bludgeon |

### Deikhan

| Weapon | Damage |
|--------|--------|
| claymore | 70% cleave / 30% thrust |
| estoc | 100% thrust |
| falx | 70% thrust / 30% slice |
| flamberge | 70% slash / 30% slice |
| khopesh | 70% slash / 30% stab |
| lance | 100% thrust |
| longsword | 70% slash / 30% thrust |
| mace | 100% bludgeon |
| martel | 70% bludgeon / 30% strike |
| mattock | 70% bludgeon / 30% stab |
| maul | 100% smash |
| pike | 100% spear |
| ranseur | 70% spear / 30% thrust |
| saber | 70% slash / 30% slice |
| scythe | 70% slice / 30% slash |
| scepter | 100% bludgeon |
| tulwar | 70% slash / 30% slice |
| zanbatou | 70% cleave / 30% slash |

### Cleric

| Weapon | Damage |
|--------|--------|
| baton | 100% strike |
| cudgel | 100% bludgeon |
| flail | 70% bludgeon / 30% strike |
| greathammer | 100% smash |
| hammer | 100% bludgeon |
| mace | 100% bludgeon |
| mallet | 100% smash |
| martel | 70% bludgeon / 30% strike |
| maul | 100% smash |
| morningstar | 70% smash / 30% stab |
| quarterstaff | 70% strike / 30% bludgeon |
| scepter | 100% bludgeon |
| warhammer | 70% bludgeon / 30% thrust |

### Thief

| Weapon | Damage |
|--------|--------|
| baton | 100% strike |
| cudgel | 100% bludgeon |
| cutlass | 70% slash / 30% stab |
| dagger | 100% stab |
| dirk | 70% stab / 30% thrust |
| epee | 100% thrust |
| fauchard | 70% slash / 30% spear |
| harpoon | 100% spear |
| katana | 70% slice / 30% thrust |
| katar | 100% thrust |
| khopesh | 70% slash / 30% stab |
| kris | 70% stab / 30% slice |
| kukri | 70% slash / 30% cleave |
| naginata | 70% slice / 30% spear |
| poniard | 100% stab |
| ranseur | 70% spear / 30% thrust |
| rapier | 100% thrust |
| sai | 70% stab / 30% strike |
| scimitar | 100% slash |
| shortsword | 70% thrust / 30% slash |
| stiletto | 100% stab |
| tachi | 70% slash / 30% slice |
| tanto | 70% stab / 30% slash |
| wakizashi | 70% slash / 30% stab |

### Mage

| Weapon | Damage |
|--------|--------|
| dagger | 100% stab |
| kris | 70% stab / 30% slice |
| mace | 100% bludgeon |
| misericorde | 100% thrust |
| scimitar | 100% slash |
| scepter | 100% bludgeon |
| sickle | 70% slash / 30% stab |

### Shaman

| Weapon | Damage |
|--------|--------|
| axe | 100% cleave |
| cleaver | 100% cleave |
| harpoon | 100% spear |
| hatchet | 70% cleave / 30% slash |
| katar | 100% thrust |
| kukri | 70% slash / 30% cleave |
| machete | 70% slash / 30% cleave |
| mallet | 100% smash |
| mattock | 70% bludgeon / 30% stab |
| maul | 100% smash |
| pick | 70% stab / 30% bludgeon |
| pickaxe | 70% stab / 30% strike |
| scythe | 70% slice / 30% slash |
| sickle | 70% slash / 30% stab |
| spade | 70% strike / 30% slash |
| tomahawk | 70% slash / 30% strike |
| trident | 70% spear / 30% slash |
| truncheon | 100% strike |
| war club | 70% smash / 30% slash |
| zanbatou | 70% cleave / 30% slash |

### Monk

| Weapon | Damage |
|--------|--------|
| baton | 100% strike |
| epee | 100% thrust |
| kanabo | 70% smash / 30% stab |
| mace | 100% bludgeon |
| maul | 100% smash |
| pike | 100% spear |
| quarterstaff | 70% strike / 30% bludgeon |
| ranseur | 70% spear / 30% thrust |
| rapier | 100% thrust |
| sai | 70% stab / 30% strike |
| shortsword | 70% thrust / 30% slash |
| sledgehammer | 100% smash |
| spade | 70% strike / 30% slash |
| tanto | 70% stab / 30% slash |
