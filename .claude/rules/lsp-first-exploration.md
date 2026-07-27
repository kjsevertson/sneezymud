---
paths: code/code/**
---

# LSP-First Code Exploration

When exploring the SneezyMUD C++ codebase, **use LSP tools as your primary navigation method**. clangd is running on the host and indexes the full codebase.

**CRITICAL:** FOLLOW THESE INSTRUCTIONS - IT WILL MAKE YOU MUCH MORE EFFICIENT AND EFFECTIVE AND THE USER WILL BE SAD IF YOU DON'T

## When to Use LSP Instead of Built-In Tools

### Instead of Grep for finding definitions

- BAD: `Grep pattern="readMobFromDB" path="code"` (returns noise: comments, strings, partial matches)
- GOOD: `LSP operation="goToDefinition"` from any call site (goes straight to the implementation)
- GOOD: `LSP operation="workspaceSymbol"` when you don't have a call site yet (but note: this sometimes returns empty; if it does, fall back to a targeted Grep to find one occurrence, then use goToDefinition/hover from there)

### Instead of Grep for finding callers/references

- BAD: `Grep pattern="generateHeight"` (returns definitions, declarations, comments, string mentions)
- GOOD: `LSP operation="findReferences"` (returns only actual code references, compiler-precise)
- EVEN BETTER: `LSP operation="incomingCalls"` (follows the full call hierarchy, not just direct references)

### Instead of Read for understanding a function's signature and context

- BAD: `Read file_path="..." offset=X limit=Y` then scanning for the function (wastes context on surrounding code)
- GOOD: `LSP operation="hover"` (returns signature, return type, parameters, containing class, providing header)
- GOOD: `LSP operation="documentSymbol"` (returns all symbols in a file - like a table of contents)

### Instead of Grep for tracing call chains

- BAD: Grep for function name → Read the file → Grep for next function → Read that file → ...
- GOOD: `LSP operation="outgoingCalls"` (what does this function call?)
- GOOD: `LSP operation="incomingCalls"` (what calls this function?)
- Chain these to trace execution paths with minimal context usage

### Instead of Grep for understanding class hierarchy

- BAD: Grepping for class names across header files
- GOOD: `LSP operation="hover"` on a class name (shows inheritance)
- GOOD: `LSP operation="goToImplementation"` for virtual methods

## When LSP Cannot Help (Use Built-In Tools)

- **Documentation**: `docs/systems/**/*.md` files - use Grep/Read (not code, no LSP server)
- **Data files**: `lib/**` - static text files - use Read
- **String/SQL searches**: Editor menu text, SQL table names, error messages - use Grep
- **Understanding logic**: After LSP navigates you to the right function, you still need Read to understand what the function body _does_ (LSP tells you structure, not semantics)
- **Broad discovery**: "Which files deal with mob loading?" - start with Grep or docs, then switch to LSP once you have a foothold

## The Optimal Exploration Pattern

1. **Orient via docs**: Search `docs/systems/**/*.md` file frontmatter for relevant keywords/symbols
2. **Find a foothold**: Use Grep to find one occurrence of a key symbol in code
3. **Navigate via LSP**: From that foothold, use goToDefinition, findReferences, incomingCalls, outgoingCalls to trace the full picture
4. **Read for comprehension**: Use Read only on the specific functions/sections you've identified as relevant
5. **Verify completeness**: Use findReferences to confirm "is this the ONLY place X is called?" rather than hoping your Grep caught everything

## Key Insight

Each Grep returns pages of results that fill context. Each LSP call returns precise, compiler-verified results. A typical exploration that takes 15+ Grep/Read calls can often be done in 5-6 LSP calls plus 2-3 targeted Reads. This is not just faster - it prevents context compaction, which is the primary cause of inaccurate results from subagents exploring this codebase.

## When LSP Calls Fail

A failed LSP call is **not permission to abandon LSP**. LSP failures are usually transient or caused by fixable mistakes - do not silently fall back to Grep/Read after a single failure.

**Troubleshoot before falling back:**

1. **Wrong position**: The most common cause. `character` must land ON the symbol, not on whitespace, punctuation, or a keyword before it. Re-read the line and count carefully. Try hover on the same position first - if hover returns nothing, your coordinates are wrong.
2. **Server not ready**: The language server may still be indexing. Wait a moment and retry the same call.
3. **Wrong operation for the context**: `goToDefinition` on a definition returns itself. `incomingCalls`/`outgoingCalls` only work on functions/methods, not types or variables. `goToImplementation` is for interfaces and abstract members, not concrete functions. Try a different operation.
4. **File not saved or has errors**: If the file has unsaved changes or syntax errors, LSP results may be degraded. Check `mcp__ide__getDiagnostics` for the file.
5. **workspaceSymbol quirks**: This operation sometimes returns empty even for symbols that exist. This is a known limitation - fall back to Grep for one occurrence, then use goToDefinition from there.

**The escalation path:**

- First failure: Check your coordinates and retry with corrected position
- Second failure: Try a different LSP operation or a nearby symbol
- Third failure: Try hover on the same spot to verify the server is responding at all
- After 3-4 genuine attempts with different approaches: Report the problem to the user (what you tried, what errors you got, what you think is wrong), then continue with Grep/Read as a fallback

**Never do this:** Silently switch to Grep after one failed LSP call and never try LSP again for the rest of the task. The user has these instructions here because LSP is significantly more effective - giving up without troubleshooting wastes that advantage.

## LSP Tool Parameters

All operations require: `filePath`, `line` (1-based), `character` (1-based). The filePath can be relative from CWD (e.g., `code/code/sys/db.cc`). You need at least a file+position to start from - use a single Grep to find that initial position if needed, then LSP for everything after.
