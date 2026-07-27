---
paths: **/*.ts
---

# TypeScript Rules

- Never use type assertions to bypass the type system, outside of known safe cases (e.g. `as const` for literal types).
- Never disable ESLint or TypeScript rules unless no valid workaround exists. If you must disable a rule, add a comment explaining why it's necessary. If there's a valid TypeScript way to achieve the same result, use that instead.
- Code should be written to be as self-documenting and intuitive as possible.
- Always look for places to add comments explaining intent and rationale when decisions are non-obvious to future maintainers.
- Never add comments that simply re-state what the code is doing. Comments should have a clear purpose.
- Always run `bun run check` (from `tests/functional/`) before considering your work complete. Fix anything that surfaces and repeat until clean.
- Always export functions/variables where they're declared. Don't use `export { foo }` at the bottom of the file. This makes it easier to see what's being exported and prevents accidentally forgetting to export something.
- Don't add section identifier comments. If a file is large enough to need them, it should be split into smaller files.
- If a function takes more than one argument, use an options object instead. This makes it easier to understand what each argument is for and allows for more flexible function signatures in the future.
