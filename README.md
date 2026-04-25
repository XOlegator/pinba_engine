# Pinba Engine (MySQL 8+ Fork)

Pinba Engine is a MySQL storage engine used to collect and analyze PHP runtime statistics sent over UDP.

This repository is an actively maintained fork of the original Pinba Engine project:

- Original project: https://github.com/tony2001/pinba_engine

## Project Goals

The primary goal is to keep Pinba Engine usable on modern systems, starting with MySQL 8.0+.

Current direction:

- compatibility with modern MySQL server headers and plugin APIs;
- C++23-based CMake development workflow;
- reproducible build, test, and static-analysis tooling;
- cleanup of obsolete legacy build files and documentation;
- preserving core Pinba behavior where possible.

## Build and Test

Use CMake presets for normal development:

```bash
cmake --preset release
cmake --build --preset release
ctest --test-dir build --output-on-failure
```

The resulting plugin is `build/ha_pinba.so`.

## Documentation

- Build, dependencies, tests, and local plugin install: `docs/build.md`
- Docker usage: `docs/docker.md`
- Release flow and commit-message rules: `docs/releasing.md`
- Roadmap and open stabilization work: `docs/roadmap.md`
- Historical release notes from the legacy project: `docs/legacy-news.md`

## Release Process

This fork uses a semi-automated GitHub release flow with automatic `CHANGELOG.md` updates. New release notes belong in `CHANGELOG.md`; the old release notes are kept in `docs/legacy-news.md` only as historical archive.

## Project Status

This is an in-progress modernization fork. Interfaces, internals, and build details may continue to evolve while MySQL 8+ support is being stabilized.
