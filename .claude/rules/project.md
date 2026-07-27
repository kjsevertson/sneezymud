# SneezyMUD

Text-based MUD server with 30+ years of history, descended from DikuMUD. C++20 codebase undergoing modernization.

## Tech Stack

- C++20 (Clang primary, GCC supported)
- MariaDB (`sneezy` and `immortal` databases)
- CMake build system with sanitizers (ASan, UBSan) and comprehensive compiler warnings enabled by default
- clangd running in host OS for LSP features (code completion, diagnostics, navigation)

## Commands

- `make` - Build (default preset: dev-clang)
- `make PRESET=dev-gcc` - Build with specific preset (dev-gcc, dev-clang, release-gcc, release-clang)
- `make rebuild` - Clean and rebuild current preset
- `make format FILE=<file>` - Format before committing
- `make test` - Run C++ unit tests via CTest
- `make test-func` - Run functional tests (requires running server)
- `make test-all` - Run all tests
- `mariadb sneezy` / `mariadb immortal` - Connect to MariaDB shell. No credentials required. Must specify database name for `-e` queries.

## Static Analysis (clang-tidy)

- `run-clang-tidy -j $(nproc)` for full-codebase sweeps. Save output to a file and grep from it rather than re-running expensive sweeps.
- Passing `-checks` on the command line makes clang-tidy ignore `HeaderFilterRegex` from `.clang-tidy`. Pass `--header-filter="code/code/.*"` explicitly alongside `-checks`.
- `clang-analyzer-*` checks only run on explicit clang-tidy invocation, not during clangd live editing.
- Many `cert-*` and `hicpp-*` checks are aliases of `bugprone-*` - don't double-count when auditing violations.
- Wildcard check specs (`bugprone-*`) silently ignore non-existent checks, so they're forward-compatible across clang-tidy versions.

## Workflow

- Address ALL linter/compiler diagnostics **including info-level** within your scope.
- Write modern C++20 in new and modified code, but don't modernize surrounding code you weren't otherwise changing — keeping PRs focused is more important than opportunistic cleanup.

## Related Repositories

- `sneezymud-docker` (`~/source/repos/sneezymud-docker`) — Docker Compose deployment. Includes database image (seeds from `_Setup-data/`), game server packaging, monitor service (auto-updates + crash recovery), web client, and buildertools.
- `sneezymud-backups` (`~/source/repos/sneezymud-backups`) — Nightly backup automation. GitHub Actions workflow SSHes to production, downloads mysqldump + mutable files archive, publishes as GitHub Releases. Server-side backup runs at 08:00 UTC via systemd timer; GitHub Actions downloads at 10:00 UTC.

## Legacy Code

- `code/code/low/sqled*.cc` - Standalone CLI tools (`sqledmob`, `sqledobj`, `sqledresp`, `sqledshop`, `sqledwld`) for offline editing of game data. These are legacy and no longer used. Still compiled via CMakeLists.txt but should not be referenced as active tooling.

## System Documentation

Comprehensive AI-generated documentation lives in `docs/systems/`, organized by risk level:

- `critical/` - Memory safety, DELETE flags, combat, movement (bugs cause crashes)
- `important/` - Combat formulas, character stats, spell systems (bugs cause gameplay issues)
- `informational/` - UI, reference data, legacy systems

**Always search `docs/systems/` before making changes or answering questions about the codebase.** Each document has YAML frontmatter with `primary_symbols` and `keywords` for searching:

```bash
grep -rl "reconcileDamage\|DELETE_THIS" docs/systems/
grep -l "primary_symbols:.*pulse" docs/systems/**/*.md
```

Read matching documentation before proceeding. Prioritize `critical/` first, then `important/`, then `informational/`.
When multiple documents match, don't stop after the first — broad topics span multiple documents. Check at least the top matches from each relevant priority tier.
This codebase is poorly organized. Related code is scattered across many files. Documentation describes correct behavior; existing code may be wrong. Always trust the docs over nearby code patterns.

## Environment & Preferences

- `.claude/` is its own git repo, separate from the main project. Commit config changes there, not in the main repo.
- Git remotes: `origin` = upstream production (`sneezymud/sneezymud`), `fork` = the working fork (`kjsevertson/sneezymud`). Development targets `fork`; `origin` is for pulling upstream and opening PRs to the team. Use `--no-track` when branching from upstream so a stray `git push` doesn't target production: `git checkout -b <branch> origin/master --no-track`

## Database

### Migrations (`code/code/sys/migrations.cc`)

Migrations are C++ lambdas in a `std::vector`, run in order and tracked by a version number in the `configuration` table. Add new migrations by appending to the vector.

- FK type mismatches cause errno 150: column types must match exactly. Check both sides with `information_schema.COLUMNS` before writing migrations.
- DDL auto-commits in MariaDB — migrations are not transactional. A crash mid-migration leaves the DB partially migrated. Idempotent migration bodies handle this automatically on restart.
- Write all migrations to be idempotent — use `IF NOT EXISTS`, `INSERT IGNORE`, existence checks. Version is bumped per-migration, but DDL auto-commit means a crash mid-migration leaves partial state.
- `ADD FOREIGN KEY IF NOT EXISTS` creates duplicate FKs with new auto-generated names on each run. Use `addForeignKey()` helper in migrations.cc which checks `information_schema.KEY_COLUMN_USAGE` first.
- `ADD UNIQUE KEY IF NOT EXISTS` (unnamed) matches by auto-generated index name, not by column combination. If an existing index has a colliding name (e.g., FK auto-creates `player_id` index), the ADD silently no-ops. Always use explicit names: `ADD UNIQUE INDEX IF NOT EXISTS my_name (col1, col2)`.
- SQL keywords in this codebase are mixed case — always use case-insensitive search (`-i`) when grepping for SQL patterns like `ON DUPLICATE`, `INSERT`, etc.

### Seed Data (`_Setup-data/`)

Database seed files used by the Docker database image to initialize fresh instances. Loaded by `setup_mysql.sh` in three phases per database (`sneezy`, `immortal`):

1. `sql_tables/` — Schema-only (DROP TABLE IF EXISTS + CREATE TABLE), one file per table
2. `sql_views/` — Database views
3. `sql_data/` — Schema + INSERT data for tables needing game world data (mobs, rooms, shops, materials, etc.)

`sql_data/` files include CREATE TABLE statements too, so phase 3 effectively re-creates those tables with data. This is fine — the DROP TABLE IF EXISTS handles it.

Tables in `sql_data/` are the curated subset of game data needed to recreate the world. Player data, account data, and other runtime state are excluded.

### Schema Gotchas

- `shopownedcentralbank` and `shopownednpcloan` have no `shop_nr` column despite their names.

### TDatabase class

All `TDatabase` instances of the same `dbTypeT` (e.g., `DB_SNEEZY`) share a **single global `MYSQL*` connection**. Each instance has its own result set state, but `begin`/`commit`/`rollback` on any instance affects ALL instances. Do not reason about transaction isolation between separate `TDatabase` objects.

- Stack-allocate `TDatabase` (never `new`) — destructor handles cleanup via RAII.
- Never use `%r` format specifier with user input — it enables SQL injection via raw/unescaped interpolation. Use `%s`.

### TTransaction class

The TTransaction class isn't well implemented and shouldn't be used. Control transactions manually.
