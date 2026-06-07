# Agent Notes

## LLM Knowledge Base

A structured knowledge base lives in `knowledge/`. Read `knowledge/SCHEMA.md` first — it defines
the entire workflow (ingest, query, lint) and directory conventions.

**Before any non-trivial task:**
1. Read `knowledge/SCHEMA.md` — conventions, frontmatter rules, what you may and may not touch.
2. Read `knowledge/wiki/index.md` — catalog of all compiled knowledge.
3. Read `knowledge/wiki/overview.md` — high-level synthesis.
4. Read relevant `knowledge/wiki/concepts/*.md` pages.

**Rules:**
- `knowledge/raw/` is **read-only** — never create, edit, or delete files there.
- `knowledge/wiki/` is **your domain** — you compile and maintain it from raw sources.
- `knowledge/output/` is for ephemeral query results — write here freely, ask user whether to file back.
- After any ingest or discovery, **update `knowledge/wiki/log.md`** and **`knowledge/wiki/index.md`**.
- When you learn something non-obvious during a task (gotcha, ABI constraint, workaround),
  consider adding it to a relevant `knowledge/wiki/concepts/*.md` page and log it.

## Project Context

Pinba Engine is a MySQL 8+ storage engine plugin for collecting and querying PHP runtime statistics sent over UDP. The active development path is CMake-based and uses C++23 for project code.

## Working Rules

- Preserve MySQL 8 plugin behavior unless a task explicitly asks for behavior changes.
- Keep changes small and focused; this codebase contains legacy C/C++ patterns and MySQL server API constraints.
- Do not add compatibility code for MySQL 5.x or pre-8.0 server APIs.
- Do not edit generated protobuf outputs unless regeneration is part of the task.
- Do not commit build artifacts, local IDE files, downloaded dependency trees, or temporary analysis output.
- Treat `src/ha_pinba.cc`, `src/main.cc`, pool/report code, and MySQL plugin descriptors as high-risk areas; verify builds after touching them.

## Dependencies

Required local tools for normal development:

- CMake 3.24 or newer.
- GCC 13+ or Clang with practical C++23 support.
- MySQL 8.0 client development files (`mysql.h`, `libmysqlclient`).
- MySQL 8.0 server source headers containing `sql/handler.h`.
- Protocol Buffers 3.x.

CMake can download small test and benchmark dependencies when `PINBA_FETCH_DEPENDENCIES=ON` (default). MySQL server source headers are large and are not downloaded by default; use `-DPINBA_DOWNLOAD_MYSQL_SOURCE=ON` only when a local extracted source tree or archive is unavailable. The pinned default MySQL source URL and hash live in `CMakeLists.txt` and should be updated intentionally.

## Build

Prefer presets:

```bash
cmake --preset release
cmake --build --preset release
```

Equivalent explicit configure command:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

If CMake cannot find MySQL server headers, pass an extracted source tree:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPINBA_MYSQL_SOURCE_DIR=/path/to/mysql-8.0
```

Or opt in to downloading the pinned MySQL source archive:

```bash
cmake --preset release-download-mysql-source
```

## Checks

Minimum check after code changes:

```bash
cmake --build build -j"$(nproc)"
./build/pinba_test
ctest --test-dir build --output-on-failure
```

For static analysis, use the generated compile database:

```bash
./scripts/run_clang_tidy.sh
pre-commit run --all-files
```

Pre-commit also references `clang-format`, `cppcheck`, `cmake-lint`, and `hadolint`; install these locally when working on relevant files.

## Branching and Pull Requests

All development must happen in a dedicated branch, never directly on `master`.

**Workflow:**
1. Create a branch from the current `master`: `git checkout -b <type>/<short-description>`
2. Make all commits on that branch following the Conventional Commits rules below.
3. Open a Pull Request targeting `master` — never push commits directly to `master`.
4. The PR title must follow the same Conventional Commits format (e.g. `fix(build): ...`).
5. Merge only after CI passes; use squash merge.

Branch naming convention: `<type>/<short-kebab-description>`, for example:
- `fix/snprintf-overflow-pinba-map`
- `feat/mysql-version-monitor`
- `docs/update-agents-branching-rules`
- `chore/update-vendor-headers`

## Commit Messages

All commit messages and PR titles/descriptions must be written in **English**.

All knowledge base entries under `knowledge/wiki/` must also be written in **English**.
Russian or any other language is not permitted in wiki pages, source summaries, or log entries.
See `knowledge/SCHEMA.md` → Language Policy for details.

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
