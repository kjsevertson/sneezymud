#!/usr/bin/env bun

// PreToolUse hook: block Edit/Write that introduces legacy C++ patterns.
// Only checks actually-changed lines, not surrounding context.
// Exit 2 = block with message; Exit 0 = allow.

const CPP_EXTENSIONS = new Set([".cc", ".h", ".cpp", ".hpp"]);

const LEGACY_PATTERNS = [
  { pattern: /\bNULL\b/, fix: "NULL → use nullptr" },
  { pattern: /\bTRUE\b/, fix: "TRUE → use true" },
  { pattern: /\bFALSE\b/, fix: "FALSE → use false" },
  { pattern: /\b(?:sprintf|snprintf)\b/, fix: "sprintf/snprintf → use boost::format (or std::format if file already uses it)" },
  { pattern: /\b(?:strcpy|strcat)\b/, fix: "strcpy/strcat → use std::string/sstring operations" },
  { pattern: /\bnew\s+TDatabase\b/, fix: "new TDatabase → stack-allocate (RAII)" },
] as const;

interface HookInput {
  tool_name: string;
  tool_input: {
    file_path?: string;
    filePath?: string;
    old_string?: string;
    new_string?: string;
    content?: string;
  };
}

function extname(path: string): string {
  const dot = path.lastIndexOf(".");
  return dot >= 0 ? path.slice(dot) : "";
}

// Multiset diff: returns lines in newStr that aren't accounted for by oldStr.
// Each identical old line "consumes" one new line occurrence, so unchanged
// context lines are excluded even if they appear multiple times or reorder.
function changedLines(oldStr: string, newStr: string): string[] {
  const oldCounts = new Map<string, number>();
  for (const line of oldStr.split("\n")) {
    oldCounts.set(line, (oldCounts.get(line) ?? 0) + 1);
  }

  const changed: string[] = [];
  for (const line of newStr.split("\n")) {
    const remaining = oldCounts.get(line) ?? 0;
    if (remaining > 0) {
      oldCounts.set(line, remaining - 1);
    } else {
      changed.push(line);
    }
  }
  return changed;
}

const input: HookInput = JSON.parse(await Bun.stdin.text());
const filePath = input.tool_input.file_path ?? input.tool_input.filePath ?? "";

if (!CPP_EXTENSIONS.has(extname(filePath))) process.exit(0);

const lines =
  input.tool_name === "Edit"
    ? changedLines(
        input.tool_input.old_string ?? "",
        input.tool_input.new_string ?? "",
      )
    : input.tool_name === "Write"
      ? (input.tool_input.content ?? "").split("\n")
      : process.exit(0);

if (lines.length === 0) process.exit(0);

const text = lines.join("\n");
const violations = LEGACY_PATTERNS.filter(({ pattern }) => pattern.test(text));

if (violations.length > 0) {
  const msg = [
    "Legacy C++ patterns detected in new/changed lines:",
    ...violations.map(({ fix }) => `  - ${fix}`),
    "Rewrite using modern C++20 equivalents.",
  ].join("\n");
  process.stderr.write(msg + "\n");
  process.exit(2);
}
