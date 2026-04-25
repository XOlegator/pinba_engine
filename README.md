# Pinba Engine (MySQL 8+ Fork)

This repository is an actively maintained fork of the original Pinba Engine project:

- Original project: https://github.com/tony2001/pinba_engine

## What this fork is

Pinba Engine is a MySQL storage engine used to collect and analyze PHP runtime statistics sent over UDP.

The original project is effectively unmaintained, so this fork exists to continue practical development and keep the engine usable on modern systems.

## Main goal of this fork

The primary goal is to migrate and evolve Pinba Engine for modern MySQL versions, starting with **MySQL 8.0**.

Key directions:

- compatibility with modern MySQL server headers and plugin APIs;
- C++23-based CMake development workflow;
- modernization of build/release workflows;
- cleanup of legacy code and obsolete tooling;
- preserving core Pinba behavior where possible.

## How this fork differs from the original

Compared to the original upstream, this fork focuses on:

- active maintenance and issue resolution;
- modern CI/CD and semi-automated GitHub releases;
- changelog-driven releases via `CHANGELOG.md`;
- forward-looking compatibility work for MySQL 8.x.

## Release process

This fork uses a semi-automated GitHub release flow with automatic `CHANGELOG.md` updates.

- Release guide: `docs/releasing.md`

## Build and test

Use CMake presets for normal development:

```bash
cmake --preset release
cmake --build --preset release
ctest --test-dir build --output-on-failure
```

See `BUILD.md` for dependency details, including MySQL 8.0 server source headers.

## Project status

This is an in-progress modernization fork.

Interfaces, internals, and build details may continue to evolve while MySQL 8+ support is being stabilized.
