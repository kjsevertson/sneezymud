---
paths: tests/**
---

# Testing

Two test suites: C++ unit tests (Google Test) and TypeScript functional tests (bun:test).

## Commands

- `make test` - C++ unit tests via CTest (builds automatically)
- `make test-func` - Functional tests (requires running server + `tests/functional/.env`)
- `make test-all` - Both suites
- `cd tests/functional && bun run check` - Typecheck + lint functional test code

## Test quality principles

Apply these when writing new tests or evaluating whether to port/keep existing tests:

- **Test behavior, not implementation.** Assert on observable outcomes. If the test breaks from an internal refactor that doesn't change behavior, it's testing the wrong thing.
- **Protect against realistic regressions.** Ask: "what code change would this catch?" If the answer is "none that would actually happen," the test has no value.
- **Test edge cases, not happy paths.** `convertTo<int>("5")` exercises the standard library, not your code. Test the boundaries where your code makes decisions.
- **Require meaningful assertions.** "Doesn't crash" and "doesn't throw" are not useful assertions in an ASan-enabled codebase. Every test must assert something about the code's behavior.
- **Keep setup proportional to value.** 100 lines of setup for 3 assertions means the test belongs at a different layer or the code being tested needs refactoring.
- **No global state pollution.** Tests that modify globals or `chdir()` create invisible coupling. Each test must be independently runnable.
- **No sequential coupling.** Each `TEST()` function must be independent. Never rely on execution order or side effects from other tests.
- **No tautologies.** Don't push X into a container and assert X comes back. Don't construct an object and assert it's non-null.

## Choosing the right test layer

**C++ unit tests** (`tests/cpp/unit/`) - Pure logic, or code that needs game objects but not a database or running server. Tests link against `sneezy_lib`. For tests needing characters/rooms/components, use `GameFixture` from `unit/game_fixture.h`. Examples: string utilities, parsers, `act()` routing, `findComponent()` search, `parseSpellNum()` resolution.

**Functional tests** (`tests/functional/`) - End-to-end behavior through the game interface. Requires a running server and database. Examples: shop transactions, character creation, combat, tell routing.

If a unit test needs heavy setup (loading characters from pfiles, initializing rooms/races), either:
1. Use `GameFixture` if it only needs game objects (no database/server)
2. Move it to the functional suite where full game state exists naturally, or
3. Refactor the code being tested to extract pure logic into a separately testable function

Option 3 is preferred - it improves both the code and the tests.

## C++ Unit Tests (`tests/cpp/`)

- Google Test framework with `TEST(SuiteName, TestName)` macro.
- New `*_test.cc` files in `tests/cpp/unit/` are discovered automatically by CMake (re-run `cmake --preset dev-clang` after adding).
- Tests link against `sneezy_lib` for access to game code.
- Run a single test: `ctest --test-dir build/dev-clang -R TestName --output-on-failure` or `./build/dev-clang/tests/cpp/sneezy_tests --gtest_filter="Suite.Test"`.
- See `tests/cpp/README.md` for detailed guidance including GameFixture API and troubleshooting.

## Functional Tests (`tests/functional/`)

Integration tests drive a live MUD server over TCP using `MudClient` (`harness/client.ts`).

### Writing Tests

- Import `describe`/`it`/`expect` from `bun:test`.
- `MudClient.connect()` + `login()` in `beforeAll` (30s timeout). `mud.close()` in `afterAll`.
- `mud.command()` for regular output, `mud.pagedCommand()` for paged output.
- Account lifecycle: `withEphemeralAccount()` from `harness/accounts.ts`, 120s timeout (`ACCOUNT_TEST_TIMEOUT`). Use `loadConnectionConfig()` (not `loadConfig()`) since these tests don't need shared login credentials.
- For finer-grained account control: `createAccount()`, `deleteAccount()`, `loginAndRent()`, `attemptLogin()`, `uniqueIdentity()` are all exported from `harness/accounts.ts`. Use these when `withEphemeralAccount` doesn't fit (e.g. `tell.test.ts` needs manual create/delete because it creates two characters on one account).
- Multi-character accounts: `addCharacterToAccount()` from `harness/accounts.ts` for tests needing two characters on one account.
- Pre-login menu interactions: `mud.rawSend({ text })` sends a command and reads idle output without prompt detection. Unlike `command()`, it tolerates closed connections (returns ""). Used for account menu flows where there's no in-game prompt.
- Immortal setup: `setupCharacter()` from `harness/setup-character.ts` for configuring test characters via `@set` commands. Supports `level` and `disciplines` (array of `{index, value}`). Requires an already-logged-in immortal `MudClient`.

### Assertions

Use custom matchers from `harness/matchers.ts`, not `.includes()` or `.toContain()` (MUD output casing is unpredictable):

- `toContainCaseInsensitive(str)` - case-insensitive substring
- `toContainAnyCaseInsensitive(candidates)` - any candidate present
- `toMatchPatternCaseInsensitive(regex)` - regex with auto `i` flag

### Conventions

- Tests run serially (`maxConcurrency = 1`). Do not parallelize.
- Self-contained per file - no cross-file execution order dependencies.
- Note required immortal powers at the top of test files.
- Prompt detection's `endsWith(">") && !endsWith("->")` fallback is correct by design. Do not change it.

### Account Menu Flow (pre-login)

Menu options: "c" connect character, "a" add new character, "d" delete, "m" MOTD, "e" exit.
Delete submenu: "1" delete entire account, "2" delete single character, "3" return to menu.

### Useful Command Error Patterns

- `cast` with unknown spell: "No such spell exists." (parser failed)
- `cast` with known but unlearned spell: "You don't know that spell!" (parser succeeded)
- `tell` to offline player: "You fail to tell to '<name>'"
- `tell` to offline alt (immortal only): additionally "logged in under the same account"
