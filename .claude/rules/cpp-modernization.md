---
paths: code/code/**
---

# C++ Modernization Rules

Write modern C++20 in all new and modified code, even when every surrounding line uses the old form. Don't modernize surrounding code you weren't otherwise changing - keep PRs focused.

## Legacy Patterns to Replace

The codebase is saturated with legacy C/C++98 patterns. Never add more. Use the modern form in all new and modified code:

- **Casts:** `static_cast`/`dynamic_cast`/`reinterpret_cast`, not C-style casts (~2,500 in codebase)
- **Null:** `nullptr`, not `NULL` (~2,200 in codebase)
- **Booleans:** `true`/`false`, not `TRUE`/`FALSE` - these are `const bool` aliases in `structs.h`, used ~10,000+ times (especially `act()` calls)
- **Strings:** `sstring`/`std::format`/`boost::format`, not `sprintf`/`char buf[]` (~2,400 sprintf, ~1,100 char buf declarations)
- **C string ops:** `std::string` methods (`==`, `.empty()`, `+=`), not `strcmp`/`strcpy`/`strcat`/`strlen`
- **Arrays:** `std::array`/`std::vector`, not C arrays
- **Constants:** `inline constexpr`, not `#define`
- **Linkage:** Anonymous namespaces, not file-scope `static`
- **Scoping:** `if`-with-initializer for `dynamic_cast` and similar - scope the variable to where it's valid
- **Decomposition:** Structured bindings (`auto [key, val]`) over `.first`/`.second`
- **Attributes:** `[[nodiscard]]` on new functions where ignoring the return is always a bug

## Project-Specific String Formatting

`sstring` inherits `std::string`. For formatted output, prefer `boost::format` unless the file already uses `std::format`:

```cpp
// boost::format (no boost:: namespace needed) - preferred default
ch->sendTo(format("%s hits %s for %i damage.") % ch->getName() % vict->getName() % dam);

// std::format - use when the file already uses it
ch->sendTo(std::format("{} hits {} for {} damage.", ch->getName(), vict->getName(), dam));
```

For loops building large output with mixed types, prefer `std::ostringstream` with a single `.str()` call at the end over repeated `+=`. For a small fixed number of concatenations (2-3), simple `+` or `+=` is fine.

## Non-Owning Types: Use Where Natural, Don't Force

`std::string_view`, `std::optional`, and `std::span` clarify intent and avoid copies. Use them in new functions and contained call graphs. Don't force them where they'd cascade through existing interfaces.

**Use when:**

- Writing new functions from scratch
- Parameters/returns are read-only with a small, contained call graph
- Replacing sentinel values (`-1`, magic numbers) with `std::optional` in new code
- Replacing pointer+length parameter pairs with `std::span` in new code

**Don't force when:**

- The value gets passed to existing APIs taking `const sstring&`, `const std::string&`, or `const char*` - constructing a temporary to satisfy the callee defeats the purpose
- Changing the signature cascades through virtual methods or callers across many files - that's a dedicated refactor, not incidental modernization
- The function stores the value - these types don't own their data
- The value is already a pointer - `nullptr` expresses absence; `std::optional<T*>` adds nothing
- The function needs to resize a container - `std::span` is a view, not an owner

## General Style

- Range-based `for`, `<algorithm>`, `<ranges>`, `std::views` over index loops
- `constexpr` aggressively - C++20 supports it almost everywhere
- Constructors over init functions; member initializer lists; `default`/`delete`; `override`/`final`; rule of zero/five
- Smart pointers for new internal-only ownership; DELETE flags when ownership crosses existing public APIs
