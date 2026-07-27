#!/usr/bin/env bash
set -euo pipefail

main_repo="$HOME/source/repos/sneezymud"
worktree_base="$HOME/worktrees/sneezymud"

if [[ $# -lt 1 ]]; then
    echo "Usage: sneezy-wt-create <branch> [--from <base-branch>]"
    echo ""
    echo "Creates a new git worktree with build artifacts and config from the main repo."
    echo "  <branch>        Branch to check out (or create if it doesn't exist)"
    echo "  --from <base>   Create new branch from <base> (default: sneezymud/master)"
    echo ""
    echo "Examples:"
    echo "  sneezy-wt-create helpfile_enhancement          # existing branch"
    echo "  sneezy-wt-create fix/my-experiment              # new branch from sneezymud/master"
    echo "  sneezy-wt-create fix/my-experiment --from main  # new branch from specific base"
    exit 1
fi

branch="$1"
base_branch="sneezymud/master"
if [[ "${2:-}" == "--from" ]]; then
    base_branch="${3:?--from requires a branch name}"
fi

worktree_path="$worktree_base/$branch"

if [[ -d "$worktree_path" ]]; then
    echo "Error: $worktree_path already exists"
    exit 1
fi

mkdir -p "$worktree_base"

# Create the worktree. If the branch exists, check it out; otherwise create it.
if git -C "$main_repo" show-ref --verify --quiet "refs/heads/$branch" 2>/dev/null ||
   git -C "$main_repo" show-ref --verify --quiet "refs/remotes/$branch" 2>/dev/null ||
   git -C "$main_repo" show-ref --verify --quiet "refs/remotes/sneezymud/$branch" 2>/dev/null; then
    git -C "$main_repo" worktree add "$worktree_path" "$branch"
else
    git -C "$main_repo" worktree add "$worktree_path" -b "$branch" "$base_branch" --no-track
fi

# Symlink shared config so the worktree uses the same Claude Code and VS Code setup
ln -s "$main_repo/.claude" "$worktree_path/.claude"
ln -s "$main_repo/.vscode" "$worktree_path/.vscode"

# Regenerate cmake build so file paths are correct for the new worktree
preset="dev-clang"
(cd "$worktree_path" && cmake --preset "$preset" >/dev/null 2>&1) && \
    echo "Regenerated compile_commands.json for $preset" || \
    echo "Warning: cmake reconfigure failed - run 'cmake --preset $preset' manually"

echo "Worktree ready at $worktree_path"
