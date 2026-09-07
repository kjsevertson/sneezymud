# Vault mining: hidden chests and crypt doors

Status: **design in progress.** Nothing is implemented. This document exists so
the work can be picked up on another machine without redoing the exploration.

## The feature

Mining can turn up something other than ore. A single discovery roll, gated by
depth, yields one of two outcomes:

- **A hidden chest** — a container of level-appropriate loot, found in the rock.
- **A crypt door** — a portal into a small micro zone that exists only for a
  while and then collapses.

One system, one set of odds, two outcomes. Not two features that happen to share
a trigger.

## Decisions already made

These were settled in conversation. Don't relitigate them without asking.

- **One discovery system, two outcomes.** A single "what did the pick turn up"
  roll decides both whether anything was found and which of the two it is.
- **Depth-scaled odds, no hard floor.** Every mineable room is eligible. The
  chance climbs with `getMineDepth(rp)`; deep rock is dramatically better but
  shallow rock is never excluded. See the depth distribution below for why a
  hard threshold was rejected.
- **The entry portal decays on a timer** — time-based, not charge-based. It is
  not consumed by use.
- **The return portal inside the crypt persists.** Even if the entry portal
  decays or is destroyed, anyone inside can still get out. This is a hard
  requirement, not a nicety.
- **The crypt has its own timer.** On expiry the micro zone is destroyed and any
  players still inside are ejected to the room of origin — the room they mined
  in.
- **Crypts are pre-authored templates, cloned per discovery.** Builders author
  crypt layouts as real rooms in the live DB. Each discovery clones a template
  into fresh runtime vnums; the clone is what players enter, and the clone is
  what gets destroyed. Templates are never touched.

## Constraints found in the code

Verified against the tree at `fb4ec9c19`. These are the facts that decide what
is actually buildable.

### The world is not instanced

`room_db` is a single flat `TRoom*[WORLD_SIZE]` array, `WORLD_SIZE = 50000`
(`code/code/sys/db.h:93`). There is no instancing of any kind anywhere in the
codebase. Two parties in the same crypt vnum are in the same room. This is why
the design clones rooms rather than instancing them.

### Runtime rooms can be created and destroyed

- `CreateOneRoom(int loc_nr)` — `code/code/misc/create_rooms.cc:3277`. Allocates,
  assigns a zone by walking `zone_table`, names the room its vnum, sets sector
  `SECT_ASTRAL_ETHREAL`.
- `~TRoom` — `code/code/misc/structs.cc:512`. Nulls `room_db[in_room]`, deletes
  `dir_option[]`, deletes contained non-PC things, and **logs a bug if a PC is
  inside**. Ejection must therefore happen before teardown, not as part of it.
- Exits are created at runtime with `new roomDirData()` on `rp->dir_option[dir]`;
  precedent at `create_rooms.cc:1804` (`finishRoom`) and
  `code/code/spec/spec_rooms.cc:1125`.

**Gotcha:** `CreateOneRoom` sets `ROOM_SAVE_ROOM` by default
(`create_rooms.cc:3305`). That flag persists floor contents to
`lib/mutable/roomdata/<vnum>`. A clone must clear it, or expiring crypts leave
orphaned save files and a recycled vnum inherits the previous crypt's junk.

### There is a free vnum block

vnums **46620–49999** contain zero rooms — verified by parsing `db/sneezy/room.sql`.
Zone "Last Block" (`lib/zonefiles/46620`) covers the range with an empty reset
table and `reset_mode 2`, but is `enabled = 0`.

Runtime-created rooms do not persist across reboot. `RoomSave`
(`create_rooms.cc:2939`) writes to the *immortal* builder DB, not the live
`DB_SNEEZY` that `bootWorld` reads. For ephemeral crypts that is correct
behaviour, not a problem.

### Portals already do everything the crypt door needs

`TPortal` / `TSeeThru` — `code/code/obj/obj_portal.cc`, `obj_seethru.cc`.

- Destination: `setTarget(int vnum)` / `getTarget()`. Positive value is a room vnum.
- Charges: `setPortalNumCharges(-1)` or `0` means **unlimited** — `enterMe` only
  decrements when charges >= 1. This is how the return portal avoids being
  consumed.
- Decay: `obj_flags.decay_time`; `TPortal::objectDecay()` returns `DELETE_THIS`,
  driven from `code/code/misc/periodic.cc:1978`. This is the entry portal's timer.
- `enterMe` (`obj_portal.cc:355`) null-checks the target room and refuses
  gracefully with a "swirling vortex" message if it is gone — so a player
  entering a just-expired crypt fails safely.

**Model to copy:** `dayGateRoom`, `code/code/spec/spec_rooms.cc:1639` — loads a
portal prototype, retargets it at runtime, sets charges and type, drops it in the
room.

### The chest loot generator is private

`chestLoadOut.cc` exposes only `chestLoadOut(zoneData&)` and
`mobBagLoadOut(TMonster*)`, both called solely from zone reset
(`code/code/sys/db.cc:3330, 3993, 4004`).

The function that actually fills a container — `fillContainer(cont, level, cap)`
at `code/code/misc/chestLoadOut.cc:239` — is in an anonymous namespace. It must
be **extracted into the header** (e.g. `fillLootContainer(TOpenContainer*, int level)`)
before the chest half can reuse it. Its helpers (`containerCapacity` :193,
`rollLootKind` :115, `addLootTableItem` :223, `maybeAddRarePotion` :171,
`chestMoney` :214) are private alongside it.

`bulkLoadOutItem(classIndT, int level, race_t)` (`code/code/misc/bulkLoadOut.h:123`)
*is* public — the "one level-appropriate item" primitive.

New object prototypes are added by **migration**, not `obj.sql` — see
`code/code/sys/migrations.cc:3093` where `MINE_ORE_VNUM = 29545` is created.
A chest or portal prototype follows that pattern.

### Mining state is thin and does not persist

`unsigned short minedOut` (`code/code/misc/room.h:175`), accessors at `:249-250`.

- Only ever set to literal `1`, only ever tested for truth. **16 bits are
  effectively free** if per-room discovery flags are wanted.
- **Never persisted.** No DB column; resets to 0 on every reboot.
- **Never regrows.** Unlike `fished` and `logsHarvested`, which decay via
  `procFishRespawning` (`task_fishing.cc:563`) and `procReforestation`
  (`task_logging.cc:268`), there is no mining regrowth process and no registry
  of mined rooms.

Consequence: every mineable room is a single lottery ticket per boot. Whether
that is the intended feel is an open question below.

### Depth distribution — why the gate scales

Parsed from all 19,209 rows of `db/sneezy/room.sql`. 2,933 rooms are in mineable
sectors (hills, mountains including `SECT_VOLCANO_LAVA`, and the three caves).

| Mine depth | Rooms |
|---|---|
| 0 | 817 |
| 1 | 1301 |
| 2 | 533 |
| 3–8 | 202 |
| 9+ | 80 |

**90% of mineable rooms sit at depth 0–2.** A hard floor at depth 9 would reach
80 rooms in the entire world. This is why the depth gate scales rather than
thresholds. `getMinedOutChance` already uses the shape `max(1, 61 - 10*(depth/3))`
(`code/code/task/task_mining.cc:56`) if a similar curve is wanted.

Deepest mineable rooms (depth 16) are vnums ~10370–10395 and 27962–27965, the
latter already themed "Mining Tunnel" / "Massive Cavern".

### The unsolved technical problem: populating a clone

A cloned room at vnum 46700 is in **no zonefile's reset table**, so nothing
populates it. Zone resets are driven by `resetCom` entries keyed to a zone's vnum
range (`zoneData::resetZone`, `db.cc:3926`).

So a crypt's contents must come from somewhere else: either the template's mobs
and objects are copied along with the rooms (nothing in the codebase copies live
mobs — this would be new), or the crypt code spawns contents explicitly after
cloning. This is the main piece of genuinely new machinery, and how much work it
is depends entirely on what a crypt is supposed to contain.

Related: room spec procs are assigned at boot from
`select vnum from room where spec != 0` (`db.cc:611`). A clone that needs a spec
proc must be pushed onto `roomspec_db` manually.

### Where mining hooks in

`code/code/task/task_mining.cc`:

- `mining_pulse()` :234 — the per-swing loop. Move cost :254, skill roll :262,
  `--timeLeft > 0` :274 (so **ten successful swings per chunk**), ore at :280,
  handed to the miner :290, exp :300, mined-out roll :305.
- `revealOreFromBlast(TBeing*, int trapLevel)` :199 — the TNT path, sole caller
  `code/code/misc/trap.cc:1252` inside `trapTnt`. Already has the room and a
  level to scale by.
- `makeOreChunk(rp)` :175 — the model for building an object and placing it.

## Open questions

### Blocking — these gate the design

1. **What is inside a crypt?** Mobs, loot, a boss, something puzzle-shaped? This
   decides how the clone-population problem gets solved and whether this is a
   two-day or two-week feature.
2. **How big is a crypt?** One room or five? How many templates to author first?
3. **How does a clone get its contents** — copy from the template, or spawn
   explicitly? Follows from (1).

### Needed before implementation

4. **What does the find roll hang off** — every successful swing, or every chunk
   (one in ten swings)? The difference is a factor of ten in rarity.
5. **Does the find replace the ore or come alongside it?** Does it end the
   mining task? Does it mine the room out?
6. **Should a room stay workable after a find,** given `minedOut` never regrows
   and every room is otherwise a one-shot per boot?
7. **Chest vs crypt split** — fixed ratio, or does depth shift it (shallow rock
   mostly chests, deep rock mostly crypts)?
8. **Does blasting find things too?** `revealOreFromBlast` is the natural hook
   and "the charge opens a cavity" writes itself, but explosives were not chosen
   as a trigger.
9. **The two timer values** — how long the entry portal lasts, how long the crypt
   lives. Does the crypt clock start on discovery or on first entry?
10. **Difficulty scaling and access** — is a crypt scaled to its depth or to
    whoever opened it? What stops a low-level character opening a deep crypt and
    bringing a raid, and is that even a problem?

### Can be settled later

11. Chest loot level source — depth, zone, or miner's level.
12. Chest locked/trapped, and where the key comes from.
13. Whether the chest is handed to the miner (as ore is) or left on the floor,
    where it evaporates on reboot unless the room saves.
14. What happens to loot and corpses inside a crypt when it expires. Corpse
    recovery is the one players will genuinely be angry about.
15. Re-entry after the entry portal decays — or is the return portal strictly
    one-way out?
16. A cap on simultaneous live crypts.
17. Renting or camping inside a crypt.
18. Mining *inside* a crypt to open another one. Probably no, but the recursion
    should be closed off deliberately.

## Related work on this branch

`fb4ec9c19` wired experience into the augment tasks and mining, following
whittle's shape — `augmentTaskExp()` in `code/code/misc/augment.cc`. Mining pays
on lumberjack's pattern (`task_mining.cc:300`). Five tasks remain unwired:
bolster, serrate, hone, jam, keycut.
