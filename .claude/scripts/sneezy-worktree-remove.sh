#!/usr/bin/env bash
set -euo pipefail

main_repo="$HOME/source/repos/sneezymud"
worktree_base="$HOME/worktrees/sneezymud"

if [[ $# -lt 1 ]]; then
    echo "Usage: sneezy-wt-remove <branch>"
    echo ""
    echo "Removes a worktree and prunes git's tracking."
    echo "Does NOT delete the branch itself."
    echo ""
    echo "Available worktrees:"
    if [[ -d "$worktree_base" ]]; then
        ls -1 "$worktree_base" 2>/dev/null || echo "  (none)"
    else
        echo "  (none)"
    fi
    exit 1
fi

branch="$1"
worktree_path="$worktree_base/$branch"

if [[ ! -d "$worktree_path" ]]; then
    echo "Error: no worktree at $worktree_path"
    exit 1
fi

# git worktree remove fails with submodules, so delete manually and prune.
rm -rf "$worktree_path"
git -C "$main_repo" worktree prune

echo "Removed worktree for '$branch'"
