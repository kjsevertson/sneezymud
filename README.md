# Claude Code configuration for SneezyMUD

Backup of the `.claude/` directory used when working on [sneezymud](https://github.com/sneezymud/sneezymud) — rules, subagents, skills, hooks, and scripts.

This branch contains no game code. It exists so the config is versioned somewhere, since `.claude` is covered by a global gitignore and is otherwise unversioned on disk.

To restore, copy `.claude/` into the root of a sneezymud checkout.

`settings.local.json` is deliberately excluded — it is machine-local and contains local credentials and absolute paths.
