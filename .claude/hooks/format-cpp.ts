#!/usr/bin/env bun

// PostToolUse hook: auto-format only modified C++ lines after Edit/Write.
// Uses git clang-format to scope formatting to changed lines vs HEAD,
// so pre-existing formatting inconsistencies are left alone.

const CPP_EXTENSIONS = new Set([".cc", ".h", ".cpp", ".hpp"]);

interface HookInput {
  tool_input: {
    file_path?: string;
    filePath?: string;
  };
}

const input: HookInput = JSON.parse(await Bun.stdin.text());
const filePath = input.tool_input.file_path ?? input.tool_input.filePath ?? "";

const dot = filePath.lastIndexOf(".");
if (dot < 0 || !CPP_EXTENSIONS.has(filePath.slice(dot))) process.exit(0);

if (!(await Bun.file(filePath).exists())) process.exit(0);

Bun.spawnSync(["git", "clang-format", "--force", "HEAD", "--", filePath]);
