---
name: sneezy-pr-review
description: Review a SneezyMUD PR for safety and code quality
argument-hint: "<PR# or URL> [--post]"
disable-model-invocation: true
allowed-tools:
  - Bash(gh api:*)
  - Bash(gh pr view:*)
  - Read
  - Grep
  - Glob
  - Task
  - LSP
---

# SneezyMUD PR Review

Review pull request: $ARGUMENTS

## Step 1: Fetch PR Metadata

Use `gh pr view` (with `--repo sneezymud/sneezymud` if needed) to get:

- PR number, title, description, and author
- The head commit SHA (needed if posting comments later)

If no PR number was given, check the current branch with `gh pr view`.

## Step 2: Launch Review Agents in Parallel

Launch **both** agents simultaneously (in a single message with two Task tool calls). Each agent is self-sufficient - it will obtain the diff and investigate the codebase on its own. Do not pass the diff or pre-gathered context.

Pass each agent:

- The PR number
- Any relevant context from the conversation that the agents cannot access themselves (e.g., specific concerns the user mentioned, areas of focus, or background on the PR that they won't get from simply reading the PR via `gh`)

Do not pass your own analysis, opinions, or preliminary conclusions about the code - let the agents form their own judgments independently.

**DO NOT** tell the agents what to look for - their agent definition covers all of that. You only need to provide them the information they can't get anywhere else.

## Prompt

Tell each agent to review PR #N using `gh pr diff`.

## Step 3: Verify and Cross-Reference

After receiving both agents' results, verify their claims before consolidating:

1. **Identify overlapping findings.** When both agents flag the same issue with different analyses, read the actual source code to determine which analysis is correct. Do not default to trusting either agent.
2. **Verify specific claims.** For each claim, read and trace the code yourself until you can confirm or dispute the claim with a high level of confidence.

Specific gotchas to watch for:

- Claims about function return values are the #1 source of confidently wrong agent findings. If an agent says a function "returns X on path Y," read the function and trace that path yourself. Agents routinely get these wrong in ways that sound completely plausible.
- If an agent cites `docs/systems/` as evidence, verify the doc's claim against the actual code. The docs are AI-generated and can be wrong or outdated.
- If an agent flags a convention violation, grep the codebase to confirm the convention actually exists (not just in docs).

3. **Mark verified vs. unverified findings.** Carry verification status into the consolidation step.

## Step 4: Consolidate

Merge findings from both agents. When deduplicating overlapping findings, keep the more precise and verified analysis. If an agent made an incorrect claim, do not include it even partially; replace it with your verified analysis.

Classify each verified finding:

- **Bug/Crash**: will or could cause server crashes, incorrect behavior, or data corruption. Blocks merge.
- **Design**: maintainability, architecture, consistency. Should fix but whether it blocks merge is a judgement call on your part.
- **Quality**: DRY, comments, style, naming. Nice-to-have. A small amount shouldn't block a merge, but a PR inundated with these probably needs cleaned up. Another judgement call on your part.
- **FYI**: Non-blocking observations the author might find useful for learning purposes. These are not requests for changes - just the sort of thing a more experienced colleague might point out to a peer to be helpful.

**Do not silently drop verified findings.** Every finding that survives verification in Step 3 should be classified into one of these four tiers. Your job is to classify and frame appropriately, not to filter out lower-priority items. The FYI tier exists precisely so that minor observations have a home rather than being discarded.

**All non-FYI findings should be inline comments.** If a finding appears at multiple locations, leave the full analysis on the first occurrence and brief back-references at each subsequent location (see Inline Comments below). If a finding targets a line between diff hunks, anchor the comment on a nearby hunk line (see Inline Comment Constraints below).

Because the remote repo's PR rules require resolving all conversations to merge a PR, FYI items should go in the body rather than as inline comments.

### Evidence Standards

Every finding in the posted review must be grounded in verifiable code analysis:

- **Cite codebase conventions by example**, not by referencing docs or config files. E.g., "every other callsite of `reconcileDamage` in the codebase uses `== -1`" rather than "the docs say to use `== -1`."
- **Only flag includes the PR added.** This project has messy transitive include dependencies. Recommending removal of pre-existing includes based on clangd warnings could break compilation in other files. If the PR added a new `#include` that appears unused, that's fair game.
- **Suggest running `clang-format` on specific files for formatting issues** rather than calling out individual spacing or brace problems.
- **Filter out `.claude/`-only findings.** The review agents may produce findings based on rules in `.claude/rules/` (e.g., C++ modernization, naming conventions). These are useful when reviewing your own code, but other maintainers don't have this config. When consolidating agent output for a PR review, drop any finding that is ONLY justified by `.claude/` rules and has no independent basis in the codebase's own observable patterns. If a finding happens to align with both `.claude/` rules and actual codebase conventions, keep it but cite the codebase convention, not the config.

## Step 5: Present to User

**If the user included `--post` in their original arguments/prompt, or explicitly asked to post in the prompt, skip this step.**

Output two different reports to the user:

1. Findings organized by tier. Include file/line reference, explanation, why it matters, and a concrete suggestion for fixing. FYI items can be line-item summaries instead, for this report, to make them easy to review.

2. A presentation exactly as you'd write it when submitting the review to the actual PR (see step 6 below). This allows the user to see how the review in the PR will read before it's actually created. This is important, as PR reviews can't be deleted once created - only dismissed as stale.

For FYI items, keep it brief but detailed enough to be helpful, including the file/line reference. Frame them as things you're presenting just in case they feel like addressing them.

## Step 6: Optionally Post to PR

**Only post if the user included `--post` in their arguments or explicitly asks to post.**

### Review Body

The body is **not a summary of the review findings**. Inline comments carry the detailed analysis; the body exists only for a short introductory comment (like what a colleague would say after reading through the PR) and for FYI items.

Structure:

1. **Brief Summary of Review Outcome**: In natural language, a quick overview of the outcome of the review, leading in to a mention of inline comments and/or FYI issues if either exists in the review.
   - Examples (don't use this exact wording):
     - "Didn't find anything blocking, but did notice a couple small things I pointed out inline in case you want to address them." (Mark review as approved)
     - "Found a bug that I think needs to be addressed before merging - see the inline comment for details." (Mark review as changes requested)
     - "Don't see anything big, just some nits I'm adding below as an FYI." (Mark review as approved)
     - "Looks good all around, no issues I can see." (Mark review as approved)
2. **FYI section**: Non-blocking observations.
   - Introduced with natural language in the review outcome summary, as noted above, that makes clear these don't require action, and are only being presented in case it's useful and/or they feel like addressing them.
   - Keep each item brief but detailed enough to be helpful, including the file/line reference.

Do NOT summarize, paraphrase, or describe inline comment contents in the body, even at a high level. The reader will see them in context on the diff.

### Inline Comments

- Each inline comment should be **self-contained**, and should make sense on its own without needing the review body for context.
- Include the technical details, the "why it matters," and the suggested fix directly in each comment.
- Writing the comment in a way that would make it easy to copy and paste for an AI agent to address is a bonus, if possible without making the comment feel unnatural.

**Repeated issues:** When the same issue appears at multiple locations, put the full analysis on the first occurrence and leave brief back-references at each additional location (e.g., "Same issue as disc_thief_murder.cc:386"). Use explicit file:line references rather than "same as above" since comment ordering across files isn't predictable.

**Non-blocking inline comments:** For Design and Quality tier findings, include a qualifier like "Feel free to ignore this if you don't want to address it now" on the first occurrence so the author knows it's not blocking. Vary the phrasing naturally each time you have to do this, so it doesn't read as robotic.

### Posting Mechanics

Use `gh api repos/sneezymud/sneezymud/pulls/{number}/reviews` with:

- Inline comments on specific diff lines (requires the head commit SHA from Step 1)
- The review body as described above
- Use `REQUEST_CHANGES` if in your judgement the findings should block merge
- Use `COMMENT` if there are no blocking issues but user doesn't explicitly say to approve the PR
- Use `APPROVED` if user explicitly says something like "post this and approve it"
- FYI items never influence the review verdict
- Pass the full JSON body via `--input` from a temp file (not `-f` flags; the comments array requires JSON input)

### Inline Comment Constraints

GitHub only allows inline comments on lines that appear within diff hunks (added, removed, or context lines shown in the diff). If a finding targets a line between hunks, anchor the comment on the nearest hunk line that's contextually related to the finding, and reference the specific target lines in the comment text. This keeps the comment visually near the issue rather than exiling it to the review body.

Write the JSON to a temp file first, then verify it parses cleanly before posting. If the API returns "Line could not be resolved," the anchor line isn't in a diff hunk - find a nearby line that is, adjust the comment to reference the target lines by number, and retry.

## Tone Guidelines

- **The goal is to write naturally** like a helpful, confident human reviewer who wants their colleague to succeed.
- **Offer constructive, positively-framed feedback** while taking care not to appear condescending or patronizing
- **Write as a peer, not an evaluator.** React to the code ("I like how...", "this looks clean", "this concerns me because...") rather than issuing verdicts ("Nice work on...", "solid improvement"). The reader is a fellow developer, not a student awaiting a grade.
- **Assume shared understanding.** Don't explain back to the author why their own good decisions are good. If something is a nice DRY improvement, you can say so without explaining what DRY means or why it matters.
- Explain WHY each issue matters, not just what's wrong.
- Write comments that could serve as clear prompts for an AI assistant to fix.
- No emojis. No emdashes or double hyphens (just use single dashes surrounded by spaces).
- When pointing out a pattern violation, mention where the correct pattern exists (e.g., "backstab handles this at disc_thief_murder.cc:261").
- Distinguish blocking issues from suggestions. Don't block merges over style nits.
