---
paths: code/code/**
---

# Safety Invariants

These invariants prevent crashes and memory corruption. Detailed treatment of each topic lives in `docs/systems/critical/`. When in doubt, consult those docs.

## DELETE Flag Handling

- NEVER use `IS_SET()` for DELETE flags — use `IS_SET_DELETE()` which handles the combined bit pattern.
- NEVER check `reconcileDamage()` death with `IS_SET_DELETE` — it returns -1 on death. Check `== -1`.
- NEVER dereference pointers after operations that return DELETE_THIS — check immediately and return/break.
- Check all `int` return values for DELETE_* flags and propagate them.
- `die()` returns DELETE_THIS (self-deletion signal). Translate to DELETE_VICT when propagating to callers who passed the victim.
- Object spec procs return DELETE_ITEM, not DELETE_THIS. Check with `IS_SET_DELETE(rc, DELETE_ITEM)`.
- VICTIM_DEAD/CASTER_DEAD are spell flags, not DELETE flags — different bit positions. Magic item wrappers must translate: VICTIM_DEAD → DELETE_VICT, CASTER_DEAD → DELETE_THIS.
- Communication functions (`doSay`, `doTell`, etc.) can trigger mob AI via `checkResponses()` that returns DELETE flags. Always check return values.

## Ownership

- Return DELETE flags for caller-owned pointers; delete directly only for pointers you resolved, clearing with `REM_DELETE()`.

## Cleanup Before Deletion

- ALWAYS call `reformGroup()` before deleting any character — followers hold raw `master` pointers that will dangle.
- ALWAYS call `DeleteHatreds()` and `DeleteFears()` before deleting any character — hate/fear list entries will dangle.

## Spatial / Container

- Remove from container with `--(*item)` before deleting.
- Ensure `parent`, `equippedBy`, `stuckIn`, `roomp` are all nullptr before adding to a new location.

## Iteration

- Cache next pointer before modifying linked lists during iteration. Use `*(it++)` pattern for container deletion.
- Build a `vector` of targets first for area-effect commands, then iterate. Re-validate each target (still in room, still alive) before processing — earlier iterations may cause death/movement.

## Spells

- In area spells, when `reconcileDamage()` returns -1, delete the victim inline and continue iterating. Return SPELL_SUCCESS without VICTIM_DEAD.

## Group / Follow

- Call `circleFollow()` before `addFollower()` — circular chains cause infinite loops in master chain traversal.

## Spec Procs

- Call `swapToStrung()` before modifying mob/object names or descriptions — prototype data is shared, so modifying without stringing changes all instances.

## Assertions

- Use standard `assert()` for all runtime invariant checks. It is natively understood by clang static analyzer and all C++ tooling. Do not use `mud_assert()`.
- `NDEBUG` is never defined — release presets override `CMAKE_CXX_FLAGS_RELEASE` to drop it. `assert()` runs in all builds today.
- **O(1) checks** (pointer comparisons, field equality): always-on `assert()`, no guard.
- **O(n) scans** (walking `character_list`, `gCombatList`, etc.): wrap in `#ifndef NDEBUG`. The guard is currently inert but communicates cost and allows stripping expensive checks if production performance ever requires it.
- `vlogf(LOG_BUG, format(...))` — logs but continues. Use in destructors where aborting is unsafe.

