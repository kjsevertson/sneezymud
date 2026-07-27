---
name: self-review
description: Review local changes for safety and code quality using the SneezyMUD review agents. Use when asked to review local changes, check work before committing, or run a self-review. Supports reviewing uncommitted changes or all work since a given commit.
argument-hint: "[--staged | --unstaged | <commit-sha>]"
allowed-tools:
  - Bash(git status:*)
  - Read
  - Grep
  - Glob
  - Task
---

# Self-Review

Review local changes for safety and code quality issues.

Argument: $ARGUMENTS

## Step 1: Determine Scope

Determine the review scope from arguments:
- `--staged`: staged changes only (`git diff --staged`)
- `--unstaged`: unstaged changes only (`git diff`)
- `<commit-sha>`: all changes from that commit through HEAD, including the commit itself and any uncommitted changes (`git diff <sha>~`)
- No argument: all uncommitted changes (`git diff HEAD`)

Run `git status` (and `git log --oneline` if a SHA was provided, to confirm it exists) to validate. If there are no changes in the determined scope, say so and stop.

## Step 2: Launch Review Agents in Parallel

Launch **both** agents simultaneously (in a single message with two Task tool calls). Each agent is self-sufficient — it will obtain the diff and investigate the codebase on its own. Do not pass the diff or pre-gathered context.

Pass each agent:
- The exact diff command to use (e.g., `git diff --staged`, `git diff HEAD`, or `git diff <sha>`)
- Any relevant context from the conversation that the agents cannot access themselves (e.g., specific concerns the user mentioned, areas of focus, or background on the changes)

Do not pass your own analysis, opinions, or preliminary conclusions about the code — let the agents form their own judgments independently.

### sneezy-safety-reviewer
Tell it to review the changes using the determined diff command.

### sneezy-quality-reviewer
Tell it to review the changes using the determined diff command.

## Step 3: Verify and Cross-Reference

After receiving both agents' results, verify their claims before consolidating:

1. **Identify overlapping findings.** When both agents flag the same issue with different analyses, read the actual source code to determine which analysis is correct. Do not default to trusting either agent.
2. **Verify specific claims.** For any claim about return values, death paths, or crash scenarios, read the actual function and trace the code path yourself. In particular:
   - If an agent says a function "returns X on death," read that function to confirm.
   - If an agent cites `docs/systems/` as evidence, verify the doc's claim against the actual code.
   - If an agent flags a convention violation, grep the codebase to confirm the convention actually exists.
3. **Mark verified vs. unverified findings.** Carry verification status into the consolidation step.

## Step 4: Present Findings

Merge findings from both agents. When deduplicating, keep the more precise and verified analysis. If an agent made an incorrect claim, replace it with your verified analysis. Organize into four tiers:

### Bugs / Crashes
Issues that will or could cause server crashes, incorrect behavior, or data corruption.

### Design Issues
Maintainability, architecture, and consistency concerns.

### Quality Suggestions
DRY, comments, style, naming. Nice-to-have improvements.

For each finding, include:
- File and line reference
- Clear explanation of the issue
- Why it matters
- Concrete suggestion for fixing

### Evidence Standards

Every finding must be grounded in verifiable code analysis:

- **Cite codebase conventions by example.** E.g., "every other callsite of `reconcileDamage` uses `== -1`" rather than "the docs say to use `== -1`."
- **Only flag includes that were added by the changes.** This project has messy transitive include dependencies. Recommending removal of pre-existing includes could break compilation in other files.
- **Suggest `clang-format` for formatting issues** rather than calling out individual problems.
