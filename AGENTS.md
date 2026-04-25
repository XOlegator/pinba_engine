# Agent Notes

## Project Context

Pinba Engine is a MySQL 8+ storage engine plugin for collecting and querying PHP runtime statistics sent over UDP. The active development path is CMake-based; legacy files may remain for historical compatibility but should not drive new work.

## Working Rules

- Preserve MySQL plugin behavior unless a task explicitly asks for behavior changes.
- Keep changes small and focused; this codebase contains legacy C/C++ and MySQL server API constraints.
- Do not edit generated protobuf outputs unless regeneration is part of the task.
- Do not commit build artifacts, local IDE files, or temporary analysis output.
- Treat `src/ha_pinba.cc`, `src/main.cc`, pool/report code, and MySQL plugin descriptors as high-risk areas; verify builds after touching them.

## Build

Use an out-of-tree build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The build requires MySQL client headers/libraries and MySQL server headers containing `sql/handler.h`. If CMake cannot find server headers, pass:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPINBA_MYSQL_SOURCE_DIR=/path/to/mysql-8.0
```

## Checks

Minimum check after code changes:

```bash
cmake --build build -j"$(nproc)"
./build/pinba_test
ctest --test-dir build --output-on-failure
```

Current note: the top-level CMake file may report `No tests were found` for `ctest`; still run it and report that result.

For static analysis, generate a compile database first:

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
./scripts/run_clang_tidy.sh
```

Pre-commit configuration also references `clang-format`, `cppcheck`, `cmake-lint`, and `hadolint`; run applicable hooks when those tools are available.

## Commit Messages

Follow `docs/releasing.md`: use Conventional Commits so Release Please can calculate SemVer and changelog entries.

Allowed common types:

- `feat: ...`
- `fix: ...`
- `perf: ...`
- `refactor: ...`
- `docs: ...`
- `test: ...`
- `chore: ...`

Optional scopes are allowed, for example `fix(build): ...` or `docs: ...`. For breaking changes, use `!` after the type/scope or add a `BREAKING CHANGE:` footer.

## Release Notes

Release automation watches `master`, opens release PRs, updates `CHANGELOG.md`, and creates `vX.Y.Z` tags after the release PR is merged. Do not manually edit release output unless the task is specifically about release maintenance.
