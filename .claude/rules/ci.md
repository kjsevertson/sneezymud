# CI/CD (GitHub Actions)

## Architecture

- CI workflow: `.github/workflows/ci.yml`
- Build caching: `.github/actions/cached-build/action.yml`
- Release/Docker: `.github/actions/release/action.yml`
- Build preset used in CI: `release-clang`
- Runner: `ubuntu-24.04` (4 vCPUs)

## Caching

- Only `code/sneezy` (the binary) and ccache are cached — not `build/`. Ninja ignores cached .o files anyway (fresh checkout timestamps make them appear stale), so ccache is the only mechanism providing incremental compilation. The cached binary is used on exact cache hits to skip the build entirely.
- GitHub Actions has a 10 GiB cache limit per repo with LRU eviction. Cache entries are shared with Docker buildkit layer caches.
- `gh cache list --repo sneezymud/sneezymud` to inspect cache entries and sizes.

## Build Performance

- LTO is disabled by default (`SNEEZY_ENABLE_LTO` option exists but is OFF). MUD workload is I/O-bound; LTO provides no measurable runtime benefit but adds ~7 minutes of link time.
- Ninja auto-parallelizes based on available cores. No explicit `-j` flag needed.
- Full rebuild on CI (no ccache): ~10 min compile + <1 min link (without LTO).
- Full rebuild with warm ccache: ~1-2 min compile + <1 min link (without LTO).

## Linking

- Boost is statically linked (`Boost_USE_STATIC_LIBS ON` in `cmake/Dependencies.cmake`). The Dockerfile and CI cache-hit path do not install Boost runtime libraries — dynamic linking would break both.
