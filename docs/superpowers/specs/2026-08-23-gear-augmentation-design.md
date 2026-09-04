# Gear Augmentation System — Design

## The problem

Sneezy has ten races, six live classes, and a wide spread of desirable stats. Gear is hand-built, so a good item occupies exactly one cell of a race × class × size matrix. Making that item available to the rest of the game means a builder recreating it eight more times by hand, which does not scale and never has.

Gear augmentation attacks this from the other side. Instead of building more cells, players move an item into the cell they need.

## First principle

**This system is never the fast path.** Hunting gear remains the primary goal of playing Sneezy. Augmentation is deliberately time-intensive and expensive — a second route to success for players who have one, not a replacement for the first. Every tuning decision resolves in favor of keeping hunting faster.

## Shape

An item has a **body** and a **soul**.

The body is its tier, material, size, and AC. The soul is its stat affects. The system provides one path for each, and they never touch the same field.

```
                       ── the body ──
     Clothing  ←Gut→  Light  ←Gut→  Medium  ←Gut→  Heavy
        ↑ Plate         ↑ Plate       ↑ Plate
     (max tier capped by material hardness)

     Weave  → blank clothing, sized to any race
     Forge  → blank armor, sized to any race
     Smelt  → metal item into ingots (feeds Forge)
     Bolster → raise AC to the tier maximum for your level
               (spends soulstone charges)

     Rites  → corpse into soulstone charges (cleric only)

                       ── the soul ──
     Bangle  → item becomes jewelry (one-way)
     Distill → jewelry destroyed, yields essence
     Infuse  → essence into another item
     Transmute → change material, which raises the tier cap
```

The two halves compose into the pipeline the system exists for: take a great item in the wrong size or class, Bangle then Distill it into essence, Weave or Forge a blank in the right size, Plate it up the ladder, Bolster its AC, and Infuse the essence back in.

### Terminology

Three unrelated things in this system are graded, and none of them are the same scale. They are named apart on purpose:

| Term | Range | Belongs to |
|---|---|---|
| **Armor Tier** | Clothing, Light, Medium, Heavy | an item — derived from its `ITEM_ANTI_*` flags |
| **Soulstone Level** | 1–10 | a soulstone — how much it harvests and how cheaply it bolsters |
| **Essence Quality** | 1–N | an essence — the size of the stat bonus it can write |

Unqualified "tier" in this document means Armor Tier.

## Skills

| Class | Skill | Effect |
|---|---|---|
| Thief | Gut | Demote one tier rung |
| Deikhan | Plate | Promote one tier rung, capped by material hardness |
| Shaman | Bangle | Convert a wearable into jewelry; one item in, one out, no byproduct |
| Mage | Distill | Destroy a jewelry item, yielding its stats as essence |
| Mage | Infuse | Spend essence to add stats to an item |
| Mage | Transmute | Change an item's material |
| Monk | Weave | Create blank clothing from cloth-family commodities |
| Warrior | Forge | Create blank armor from ingots |
| Warrior | Smelt | Reduce a metal item to ingots |
| Adventuring | Bolster | Raise an item's AC to the maximum for its tier, gated on the user's level; consumes soulstone charges |
| Cleric | Rites | Extract the soul from a corpse, adding charges to a soulstone |

Bolster sits in an adventuring discipline rather than a class, because it is upkeep rather than craft. Players should not need a Bolster alt for routine maintenance. Its *fuel*, however, is cleric-only — which supplies class interdependence without gating the skill itself.

## Soulstones and Rites

Bolster consumes charges from a **soulstone**, a rechargeable item with a Soulstone Level of 1 to 10. Charges come from one place: a cleric casting **Rites** on a corpse.

This does two things the design needs. It stops Bolster from being free, and it ties the augmentation path back to hunting — the system's first principle. The slow second route is fed by the primary one rather than bypassing it.

**Yield scales with level.** This is not a tuning preference; it is what makes the corpse economy safe. PC corpses are permitted, on the reasoning that player death already costs experience and is therefore a poor way to farm charges. That reasoning holds only under level scaling — without it, a disposable low-level alt dying on repeat becomes the cheapest charge source in the game, since its XP loss is negligible.

### Yield

```
charges gained = soulstoneLevel × (mobLevel / 10)
```

A successful Rites on a Soulstone Level N stone harvests N x 10% of the corpse's level. Level 10 takes the full level; level 5 takes half.

Skill learnedness governs success or failure only — it does not scale the amount. That is deliberate: the failure rate is limiter enough, and a second multiplier would make low-skill clerics useless rather than merely slow.

The caster's own level does not enter Rites at all. Only Soulstone Level and corpse level.

### Soulstone Level

Ten levels, all on a single object vnum with level and charge count in its `val` fields — rent persists all four (`code/code/misc/rent.cc:461`), so both survive. A stone upgrades to the next Soulstone Level when it holds `level x 100` charges, and Level 10 caps at 1000.

The threshold is only checked on gain, so a stone never drops a level. Spending still costs progress, though, because charges spent must be re-earned to reach the next threshold — which puts a genuine tension between using a stone and growing it.

This scaling is self-balancing. At Level N a stone earns `N x corpseLevel/10` per Rites and needs `N x 100` to advance, so kills-per-level is `1000 / corpseLevel` regardless of Level. The grind neither accelerates nor stalls as a stone matures: roughly 20 kills per Level against level-50 corpses, about 43 against a median-level zone, and on the order of 180 kills to carry a stone from Level 1 to Level 10 before accounting for failures.

Because all ten Levels share one vnum, a Level 1 and a Level 10 stone are visually identical. Showing the Level requires `swapToStrung()` and a rename on each upgrade, the same pattern the conversions use.

### Corpse exclusivity

A corpse is worked once. Rites is mutually exclusive with sacrificing, skinning, butchering, and dissecting, in both directions.

`CORPSE_SACRIFICE` (bit 5) is not a record of past sacrifice — it is a concurrency lock set when the shaman ritual begins (`code/code/disc/disc_shaman.cc:103`) and cleared on every abort path. Sacrifice destroys the corpse, so there is no after-state to record.

That makes the exclusivity cheap to implement:

- **Rites blocks the rest.** On success it sets `CORPSE_NO_SKIN | CORPSE_NO_BUTCHER | CORPSE_NO_DISSECT`, so skinning, butchering and dissection need no changes — they already test those flags. Sacrifice is the only operation needing a new check, against a new `CORPSE_NO_RITES` (bit 10; `MAX_CORPSE_FLAGS` goes 10 → 11).
- **The rest block Rites.** Rites requires a pristine corpse: none of `NO_SKIN`, `HALF_SKIN`, `PC_SKINNING`, `NO_BUTCHER`, `HALF_BUTCHERED`, `PC_BUTCHERING`, `NO_DISSECT` set, and not `NO_REGEN`, since body parts are not corpses. One helper on `TCorpse` covers it.

One new flag and one new check outside Rites itself. See the corpse flag discipline in `docs/systems/informational/crafting-extraction.md`.

### Bolster

Bolster is a task, not an instant command, and both its difficulty and its charge cost rise with the level of the item being worked.

The gradient this produces is the intended one and needs no separate table: the AC that Bolster installs is a function of tier and level, so Heavy-tier gear at high level is inherently the expensive corner and Clothing-tier the cheap one. Monks cannot wear `TArmor` at all, so monk gear sits permanently at Clothing tier and is comparatively easy to max; high-end tank plate is the opposite extreme and should stay a long project.

As a task it inherits the usual discipline — a `CMD_TASK_FIGHTING` handler, per-pulse room and position validation, and clearing state on every exit path including interruption.

#### The ceiling

```
ceiling = tierMax × (characterLevel / 50)
```

| Tier | `tierMax` |
|---|---|
| Clothing | 30 |
| Light | 40 |
| Medium | 50 |
| Heavy | 60 |

A level-50 character reaches the full tier maximum; a level-25 character reaches half of it. Bolster runs as a task until the item's `armorLevel` reaches this ceiling, the stone runs dry, or the task is interrupted — there is no fixed tick count, the same way sharpening simply halts at maximum sharpness.

Jewelry is priced at the same rate as Clothing (`pointsPerAC` 0.85 for both), so it takes the Clothing ceiling of 30.

Each tier's ceiling lands in its own difficulty band, one step apart:

| Tier | Ceiling at level 50 | Tops out in |
|---|---|---|
| Clothing | 30 | Difficult |
| Light | 40 | Dangerous |
| Medium | 50 | Hopeless |
| Heavy | 60 | Near-impossible |

Ceilings and bands were chosen independently and align without adjustment. Monk gear, permanently at Clothing tier, never gets harder than Difficult to max; Heavy plate is the only thing that ever reaches Near-impossible.

#### Difficulty bands

Difficulty is read off the **item's** level, in seven bands matching `taskDiffT` (`code/code/misc/spell2.h:87-95`):

| Item level | Difficulty |
|---|---|
| 1–9 | Trivial |
| 10–18 | Easy |
| 19–27 | Normal |
| 28–36 | Difficult |
| 37–45 | Dangerous |
| 46–54 | Hopeless |
| 55–60 | Near-impossible |

Note this is item level, not character level — `MAX_MORT` is 50 (`code/code/misc/defs.h:48`), and `bulkLoadOut` caps generated item level at 60, so 55–60 sits above any player's own level by design.

**The band is re-read on every attempt, not fixed when the task starts.** An item's level derives from its points, so bolstering raises it. Difficulty therefore climbs as the item approaches its ceiling, and the last few points of a high-tier piece are the near-impossible stretch. That is the mechanism behind "easy to max monk gear, hard to max tank plate" — monk gear is stuck at Clothing tier and tops out in the low bands, while Heavy plate runs the full ladder into Near-impossible.

**Implementation note.** `taskDiffT` is currently a static property of a skill, read as `discArray[skill]->task`, so it describes a skill as a whole and cannot vary per attempt as written. Bolster needs its own level→band lookup in its success math, using the seven names as display vocabulary rather than reading the declared value. The enum's own comment notes it "will be obsolete eventually."

#### Charge cost

```
charges per tick = (armorLevel x 10) / soulstoneLevel
```

`armorLevel` is the item's armor-derived level on the 1–60 scale (`TBaseClothing::armorLevel()`, `code/code/obj/obj_base_clothing.cc:269`), not the raw `APPLY_ARMOR` modifier, which runs in the hundreds and clamps at 1000.

A Level 1 stone therefore pays exactly ten times what a Level 10 stone pays for the same tick:

| | Level 1 | Level 10 |
|---|---|---|
| level 20 item | 200 | 20 |
| level 55 item | 550 | 55 |

Read against a Level 10 stone's 1000-charge cap, a Level 1 stone cannot afford a single tick on a level-55 piece while a Level 10 stone gets roughly eighteen. Soulstone Level is thus the real gate on high-end work, which compounds with the difficulty bands: the expensive items are also the ones most likely to fail.

The cost was originally expressed as `armorLevel / (level x 10)`, which gives the same 10:1 ratio but collapses under integer charges - Level 10 on any item below item level 100 rounds to zero, making high-tier bolstering free rather than merely efficient, and clamping to a minimum of 1 flattens the ratio to 5:1 on trivial absolute numbers. Inverting the divisor into a multiplier preserves the intent at usable magnitudes.

Cost rises with the item's level on the same curve as difficulty, so both brakes tighten together as a piece approaches its ceiling.

A successful tick raises the item by one `armorLevel`. **A failed tick raises nothing but still burns the charge**, following the established rule for this codebase's crafting tasks — a failure path that costs nothing permits infinite retries and bypasses skill progression entirely.

Taking a Heavy piece from a blank to its level-60 ceiling on a Level 10 stone therefore costs at minimum the sum of levels 1 through 60 — 1830 charges, nearly two full stone loads at the 1000 cap. Climbing an existing level-20 piece to 55 costs 1330, still more than one load. A full max is never a single sitting.

And that is the floor, not the estimate. Failures burn charges at the current level's rate without advancing, so the real cost climbs steeply exactly where the difficulty bands bite hardest. The last stretch of a Heavy piece is charged at 55–60 per tick against Near-impossible odds, which is where the great majority of a tank piece's true cost lives.

## The tier ladder

Tier is not stored. `ArmorEvaluator::getTier()` (`code/code/obj/obj_low.cc:422`) derives it from the item's cumulative `ITEM_ANTI_*` flags, so Gut and Plate operate by moving flag bits.

Wearability is enforced in exactly one place — `TBeing::canUseEquipment()` (`code/code/misc/equip.cc:120`), reached only through `TBeing::wear()`. All eight anti-flags are read there, warrior and deikhan included. `getTier()` masks those two out, but that is a valuation concern and does not mean they go unenforced.

**Tier movement must move AC.** Without it, Plate costs you classes and returns nothing, and Gut hands out universal wearability at full heavy AC. Promotion raises AC and narrows the wearer set; demotion lowers AC and widens it. That trade is the entire point of the ladder.

The AC step per rung comes from a table owned by this system. It is deliberately **not** sourced from `pointsPerAC[]` in `obj_low.cc`, even though that table expresses a similar ratio — see Independent Tables below.

### Material hardness caps the ladder

`material_nums[].hardness` (0–100, see `code/code/misc/materials.h:125`) already exists and is used by weapons. Maximum reachable tier is a threshold table over that value:

| Max tier | Hardness | Representative materials |
|---|---|---|
| Clothing | < 20 | cloth 1, silk 7, hemp 10, fur 15 |
| Light | 20–49 | leather 20, toughened leather 25, wood 25, gold 30, copper 35, silver 40, bone 40, brass 45 |
| Medium | 50–69 | bronze 50, dragon scale 50, ivory 50, obsidian 50, platinum 50, iron 60 |
| Heavy | ≥ 70 | steel 70, mithril 80, adamantite 95, stone 95 |

The cutoffs sit on natural gaps in the data. Nothing falls between 40 and 45, and the 50 boundary separates a dense cluster (bronze, dragon scale, platinum, the hardwoods and gemlike organics) from the softer metals below it. Distribution across the four bands is 35 / 21 / 22 / 16 materials, so no tier is starved.

Known oddity, accepted: gemstones (diamond, emerald, ruby, sapphire) and stone and marble all read 70+ on raw hardness, so transmuting to any of them unlocks Heavy. That is what hardness says, even if a diamond breastplate is a strange object.

This is what makes Transmute load-bearing rather than a curiosity: it is the gate on the ladder. A woven silk shirt cannot become plate armor until a mage transmutes it to something hard enough.

### The Clothing/Light rung is a type change

`TArmor`, `TWorn`, and `TJewelry` are all siblings under `TBaseClothing`. The tier ladder is a flag operation everywhere except at the bottom rung, where it crosses a C++ type boundary:

- Gut demoting Light → Clothing must produce a `TWorn`
- Plate promoting Clothing → Light must produce a `TArmor`
- Bangle produces a `TJewelry`

C++ has no in-place type change, so each of these produces a new object rather than modifying the original. They do it by loading a template vnum — see Template Vnums below — copying the old item's state into it, splicing it into wherever the old one lived (inventory, equipment slot, or container), and deleting the old. Three skills need the identical operation, so it belongs in one shared helper. It must follow the ownership and container rules in `.claude/rules/safety-invariants.md`: remove from the container with `--(*item)` before deleting, and ensure `parent`, `equippedBy`, `stuckIn`, and `roomp` are all null before adding to the new location.

Edge case: `TArmorWand` inherits `virtual TArmor, virtual TWand`. Converting one drops the wand half. Decide whether to refuse conversion on multiply-inherited types or accept the loss.

## Template vnums

Gut, Plate, Bangle, Weave, and Forge all need a real object prototype to build from. They must not modify the original object in place.

This is a persistence requirement, not a style preference. `bulkLoadOut` currently builds gear with bare `new TArmor()` / `new TWorn()` / `new TJewelry()` (`code/code/misc/bulkLoadOut.cc:992-998`) and sets `ITEM_NORENT` at `:1001` — with no vnum there is nothing for rent to reconstruct from, so the item evaporates when the player rents. Disposable loot can live with that. Augmented gear cannot.

`db.cc:4203` is an object factory keyed on the stored item type: `ITEM_ARMOR` builds a `TArmor`, `ITEM_WORN` builds a `TWorn`. Loading from a correctly-typed template vnum gets the C++ type and the stored type field right together. Setting one without the other produces an item that behaves correctly until the player rents and then returns as its original class — a silent failure that only reproduces across a reboot.

### Wear slot does not persist, so there must be a prototype per slot

Rent persists `extra_flags`, `bitvector`, `val0-3`, `weight`, `decay`, `cur_struct`, `max_struct`, `material`, `volume`, `price`, `depreciation`, and the affects (`code/code/misc/rent.cc:461`, `:482`, `:736`, `:1088`). **`wear_flags` is not among them** — it is rebuilt from the prototype via the vnum. The `slot` column in the rent table records where the item was stored or worn, not which slots it can be worn on.

A hand-set wearable slot therefore survives only until the player rents, at which point it reverts to whatever the prototype says. `bulkLoadOut` is shielded from this purely by `ITEM_NORENT`; its generated gear never round-trips, so the bug never fires. Anything that persists cannot rely on that shield.

Two consequences:

**One prototype per slot, per type.** With a template per (slot × type), `wear_flags` restored from the prototype is correct by construction — the template *is* the slot.

**Augmentation can never change an item's wear slot.** Always load the template matching the slot the item already has. A head piece stays a head piece. Slot changing would require making `wear_flags` a persisted column first, and is out of scope.

### The block

Excluding `ITEM_WEAR_TAKE`, `ITEM_WEAR_THROW`, and the two unused bits, there are twelve wearable slots (`code/code/misc/obj.h:232-247`): Fingers, Neck, Body, Head, Legs, Feet, Hands, Arms, Back, Waist, Wrists, Hold. These are the same twelve the `BulkSlot` enum already enumerates at `bulkLoadOut.cc:233-250`.

Build all thirty-six combinations rather than pruning the odd-looking ones. Bangle is slot-preserving, so Bangling a body-slot breastplate requires a body-slot `TJewelry`; the same reasoning gives Gut and Plate a `TWorn` and a `TArmor` in every slot they touch. Gaps in the block would force the conversion helper to special-case them, which is worse than a few unused rows. The cost is seed-data plumbing in `_Setup-data/`, not design.

**Multi-slot items are a non-problem.** Conversion picks exactly one template, so an item with two slot bits would collapse to one and silently lose the other. Checked against the world data: zero items of type `ITEM_ARMOR`, `ITEM_WORN`, or `ITEM_JEWELRY` carry more than one slot bit once `ITEM_WEAR_TAKE` and `ITEM_WEAR_THROW` are masked off. The handful that look multi-slot are throwables — a shoe, a barrel lid — and `ITEM_WEAR_THROW` is not a wear slot. An assert is enough; no rule is needed.

### Status

Built. Vnums 29503-29538, created by a migration in `code/code/sys/migrations.cc`: twelve slots each of `clothing basic <slot> [bulk]` (29503-29514), `armor common <slot> [bulk]` (29515-29526), and `jewelry common <slot> [bulk]` (29527-29538). Only `wear_flag` and `type` vary across the block; every other field is copied verbatim from the existing 29925/29926 pair, and the jewelry set reuses the armor row wholesale, since conversion inherits volume, weight, price, structure, material and affects from the item it consumes.

### One mechanism

This gives creation and conversion a single shared operation: **load the template vnum for the target type and slot, then copy state into it.** Weave and Forge load a blank and fill it. Gut, Plate, and Bangle load the target type's template and copy the old item across.

Side effect worth having: augmenting a bulk-loot item gives it a real vnum, so it becomes rentable. The system quietly promotes disposable generated loot into gear worth keeping.

### Lossy

Conversions are lossy. An item Gutted and then Plated back does not return to exactly where it started.

**The loss is the integer rounding, and nothing more.** Round the resulting AC down on every conversion and the brake falls out for free — no loss table, no stored base AC, no separate tax. This is a deliberate choice not to engineer the loss: the rounding is already there, and it is enough.

One known consequence, accepted: the bite is proportionally harshest on low-AC items, since a fixed truncation is a larger fraction of a small number. Cheap items decay under repeated reshaping faster than expensive ones do.

## Essence

Distill destroys a jewelry item and yields its stat affects in portable form. Infuse spends essence to write stats onto another item.

Essence is a new object type, structured the same way as the soulstone: a single vnum carrying an apply type, an Essence Quality, and a charge count in its `val` fields. It is a placeholder, not an active item.

**`APPLY_ARMOR` is excluded from essence.** AC belongs to the body and is owned by the ladder and Bolster. If AC were essence-able it would become fungible with STR, and the two halves of the system would write to the same field.

### Eligible applies

Essence stores an apply type and a Quality, so it can only carry applies whose entire meaning is a single signed magnitude. Nineteen qualify:

| Group | Applies |
|---|---|
| Primary stats | `STR` `INT` `WIS` `DEX` `CON` `KAR` |
| Secondary stats | `BRA` `AGI` `FOC` `SPE` `PER` `CHA` |
| Pools | `HIT` `MANA` `MOVE` |
| Combat | `HITROLL` `DAMROLL` |
| Perception | `CAN_BE_SEEN` `VISION` |

`CAN_BE_SEEN` and `VISION` look behavioral but are plain additive bonuses — `canBeSeen += mod` and `visionBonus += mod` (`code/code/sys/handler.cc:465-470`).

Excluded, and why:

- `ARMOR` — body, not soul, per above.
- `HITNDAM`, `SPELL_HITROLL`, `CRIT_FREQUENCY` — deliberately out. `HITNDAM` would also double-dip against `HITROLL` and `DAMROLL`.
- `IMMUNITY`, `SPELL`, `DISCIPLINE` — carry a second parameter in `modifier2`, so they do not fit an (apply, magnitude) pair.
- `SPELL_EFFECT` — a bitmask of `AFF_` flags, not a magnitude at all.
- `LIGHT`, `NOISE`, `GARBLE`, `PROTECTION` — behavioral.
- `SEX`, `AGE`, `CHAR_HEIGHT`, `CHAR_WEIGHT` — structural. `APPLY_CHAR_HEIGHT` is commented out in `affectModify` regardless.
- `CURRENT_HIT` — transient.

The parameter-carrying exclusions are exactly the cases `getMainPointsRaw()` already special-cases (`code/code/obj/obj_low.cc:324-334`), which is a reasonable sign the line falls in the right place.

Note `PROTECTION` is mechanically identical to `VISION` and `CAN_BE_SEEN` — a plain `addToProtection(mod)` — so it is excluded by choice, not by the rule. Easy to add later if that turns out to be the wrong call.

### Distill's deposit

The deposit starts from the magnitude of the stat on the destroyed item, is **modified by the caster's skill level**, and floors at **1**. A ring carrying +5 STR yields 5 charges of STR essence at full skill and proportionally fewer at lower skill, never dropping below 1.

**Essence Quality does nothing to the deposit.** A Quality 1 essence and a Quality 9 essence receive exactly the same charges from the same item. This is the deliberate asymmetry against the soulstone: Rites yield scales with Soulstone Level, so its grind per Level stays constant, while Distill's does not. Climbing Quality therefore gets linearly harder the higher it goes, and a maximum stat stays a genuine project.

Note learnedness works differently here than in Rites, where it gates success only and never scales the amount. In Distill it scales the yield directly. The two are intentionally different.

**Every eligible affect on the item deposits.** A ring carrying +3 STR and +2 DEX yields 3 charges of STR essence and 2 of DEX — one Distill, one destroyed item, charges into as many essences as the item had eligible applies. Nothing is chosen or discarded.

**Distill merges into an existing essence of the same apply type.** Before creating one, it looks for an essence of that apply already held and adds the charges there, so repeated distilling accumulates toward the next Quality rather than scattering charges across duplicates. Only when none is found does it mint a fresh Quality 1 essence.

Together these two rules make Distill's yield strictly additive: whatever the item carried goes somewhere useful, and the somewhere is the essence the player is already growing.

### Essence Quality

Quality alone determines what Infuse can write: a Quality 1 essence adds +1, a Quality 3 essence adds +3. Charges do not enter the application at all — they exist only to grow the Quality.

An essence upgrades when it holds `quality × 10` charges, and **Quality caps at 10** — so +10 is the largest bonus Infuse can ever write. That threshold is one tenth of the soulstone's, on the same never-demotes rule: the check happens on gain, so an essence never loses Quality.

Cumulative charges to reach Quality N are therefore `5N(N−1)`:

| Quality | Charges to reach | Writes |
|---|---|---|
| 3 | 30 | +3 |
| 5 | 100 | +5 |
| 10 | 450 | +10 |

**Infuse consumes the whole essence.** Using one resets it to Quality 1 with 0 charges, no matter how large the bonus applied. So applying +2 from a Quality 7 essence throws away five levels of work — you grow an essence to exactly the size you need and never overshoot. There is no partial spending and no remainder to track.

That single rule is the brake. Concentration is expensive because a big bonus needs a quadratic amount of accumulated charge and is destroyed the instant it is used; recreation is comparatively cheap because it only needs an essence sized to the original. No separate anti-stacking rule is required.

Reading the two curves together: Quality 10 needs 450 charges, and a Distill deposits on the order of 1 to 3. That is roughly 150 to 450 items destroyed for a single +10 bonus — each Bangled first, each permanently gone, and the resulting essence consumed by one Infuse. A maximum stat is a genuine project rather than a matter of patience, which is the first principle working as intended.

## Independent tables

Every exchange rate in this system — AC per tier rung, essence per stat point, hardness thresholds — lives in tables owned by this system.

`obj_low.cc` contains tempting equivalents (`pointsPerAC[]` at line 477, `applyCost[]` at line 274). Do not reuse them. That file is the pricing system for shops and loot generation, so wiring augmentation into it denominates every augmentation in talens and makes economy tuning silently retune gameplay.

One consequence to watch, not to design around: augmented items are still *priced* by `obj_low.cc` when sold. If augmenting is cheap and the output prices high, essence becomes a money faucet. Sanity-check once real numbers exist.

## Size

Size is not a stored field. Fit is a volume comparison against the wearer's height, in `check_size_restrictions()` (`code/code/misc/equip.cc:73`):

```
perc = height × race_vol_constants[mapSlotToFile(slot)]     // doubled if ITEM_PAIRED
accept when  trunc(perc / 1.15) <= volume <= trunc(perc / 0.85)
```

Note the band is skewed upward — roughly −13.0% / +17.6%, not a symmetric ±15%, because the tolerance is applied by division rather than multiplication.

The size axis needs no skill of its own. Weave and Forge build items from scratch and set volume at creation, so a blank can be made to any race's height directly. Weave to a gnome's height and the problem is gone.

Two implementation notes. Weight is derived from volume and material density, so anything setting volume must recompute weight or weight-based checks desync. And `slot_from_bit()` picks a slot by fixed priority for multi-slot items, which may not be the slot the item is actually worn in.

### What size cannot fix

Three class restrictions are not flag-driven and cannot be moved by any amount of flag flipping (`code/code/misc/equip.cc:1870-1977`):

- Monks cannot wear any `TArmor` at all — only `TWorn` and jewelry. Material is irrelevant; the material test is dead code.
- Shamans have the same `TArmor` prohibition.
- Rangers cannot wear metal `TArmor`.

So making an armor piece monk- or shaman-wearable requires changing its C++ type, not its flags. Gut's demotion to Clothing must therefore produce a `TWorn`, not a low-tier `TArmor`, or it will not achieve what it exists to achieve.

## Open items

- **Skill checks and failure behavior.** Nothing is decided for any of the eleven skills — success rates, resource costs, what a failure destroys, whether tasks are interruptible.
- **Seed data.** `_Setup-data/` convergence for the template block and the soulstone is separate follow-up work.
