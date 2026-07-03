# Releases (semi-automated) on GitHub

## Goal

This project uses a semi-automated release flow on GitHub:

- version bump is calculated from Conventional Commits;
- release PR is created automatically;
- `CHANGELOG.md` is updated automatically;
- Git tag and GitHub Release are created automatically after release PR merge.

This replaces the old manual NEWS-driven flow, which is now archived in `docs/legacy-news.md`, and is intended for active development in the fork.

## What is automated

Configured via GitHub Actions + Release Please:

- open/update a **Release PR** to `master` **when there is at least one release-triggering commit**;
- calculate next SemVer version:
  - `major`: commit with `!` or `BREAKING CHANGE:`;
  - `minor`: at least one `feat`;
  - `patch`: `fix`, `perf`, `revert`, `deps`;
- update `CHANGELOG.md`;
- create a Git tag (`vX.Y.Z`) and GitHub Release after merge.

### Commits that do not trigger a release

With the current Release Please configuration only "user-facing" commit types trigger a release
(`feat`, `fix`, `perf`, `revert`, `deps`, and any breaking change). The following types are
**not** release-triggering on their own and will not open a Release PR by themselves:

- `docs`, `style`, `refactor`, `test`, `build`, `ci`, `chore`.

In particular, a documentation-only or CI-only change does **not** produce a new patch release.
These commits are still valid and are folded into the changelog of the next release that *is*
triggered by a release-triggering commit — they just do not start a release on their own.

## Version discipline: what warrants a release

The upstream version — the `vX.Y.Z` tag and the `VERSION` in `CMakeLists.txt` — tracks the
**shipped plugin** (`ha_pinba.so`) only. Pick a commit type by *what the change actually
affects*, not by how big it feels:

- **Plugin code and build definition** — anything that changes the compiled `ha_pinba.so`, its
  behaviour, or the set of database servers it supports: `src/**`, `pinba.proto`, `CMakeLists.txt`
  and `cmake/**`, and the vendored server headers under `vendor/**` when they add or change a
  supported MySQL/MariaDB version. Use `feat:` (a new capability or a newly supported server
  version → `minor`) or `fix:` (a bug or behaviour correction → `patch`). These cut a new version
  and tag.
- **Everything else** — distro packaging (`debian/`, `rpm/`, `docker/`), CI and workflows
  (`.github/`), helper scripts and tools (`scripts/`, `tools/`), documentation (`docs/`,
  `knowledge/`, `README.md`, `*.md`) and tests or benchmarks (`tests/`, `benchmarks/`). Use
  `ci:`, `build:`, `chore:`, `docs:` or `test:`. These do **not** bump the version: the released
  `ha_pinba.so` is unchanged, so no one repackaging our source (Debian/Ubuntu, Fedora/RPM/Copr)
  has any reason to see a new release.

Why it matters: a new tag forces every downstream repackager to rebuild an identical artefact.
Packaging or CI churn must never inflate the plugin's version.

Two rules of thumb:

- **Prefer `fix` over `feat` for corrections.** `feat` is for new capability only; making
  previously surprising or incorrect behaviour correct is a `fix` (a `patch`).
- **Publish packaging-only changes without a release.** Because they do not cut a version, ship
  them by re-running the packaging workflows manually (`workflow_dispatch` on `ppa-build.yml` or
  `rpm.yml`). For an RPM, a rebuild only needs a `Release` bump — never a new upstream `Version`.

Examples:

- Add an aarch64 RPM target → `ci(rpm): ...` (no release).
- Add support for a new MySQL/MariaDB version → `feat: ...` (`minor`).
- Fix a crash in the report pool → `fix: ...` (`patch`).
- Reword the README or a metainfo description → `docs: ...` (no release).

## Commit and PR rules

Use Conventional Commits in PR titles:

- `feat: ...`
- `fix: ...`
- `perf: ...`
- `refactor: ...`
- `docs: ...`
- `test: ...`
- `chore: ...`

Optional scope is supported:

- `feat(parser): ...`
- `fix(ci): ...`

Breaking changes:

- `feat!: ...`
- or include `BREAKING CHANGE:` in body/footer.

## Branch model

- `master` is the release branch.
- Work is done in feature branches and merged via PR.
- Release Please monitors pushes to `master`.

## Release lifecycle

1. Developers merge regular PRs into `master`.
2. GitHub Action `release-please` opens/updates a Release PR once `master` contains at least one
   release-triggering commit since the last release (see "Commits that do not trigger a release").
3. Maintainer reviews and merges the Release PR.
4. Release Please creates:
   - tag `vX.Y.Z`;
   - GitHub Release;
   - updated `CHANGELOG.md` in history.

No manual tag creation is needed in normal flow.

## One-time bootstrap and version baseline

Legacy project tags are in format `RELEASE_X_Y_Z`, while new flow uses `vX.Y.Z`.

Baseline is set in `.release-please-manifest.json`:

```json
{
  ".": "1.2.0"
}
```

So the first automated release starts from `1.2.0` baseline and creates the next `v...` tag according to new commits.

## Files used by the release system

- `.github/workflows/release-please.yml`
- `.github/workflows/pr-title-conventional.yml`
- `release-please-config.json`
- `.release-please-manifest.json`
- `CHANGELOG.md`

## What you still need to configure manually on GitHub

Do this once in your fork repository settings:

1. `Settings -> Actions -> General`
   - allow GitHub Actions for this repository.
2. `Settings -> Actions -> General -> Workflow permissions`
   - set **Read and write permissions**;
   - enable **Allow GitHub Actions to create and approve pull requests**.
3. `Settings -> Branches` (for `master`)
   - add branch protection (recommended): required PRs, required status checks.
4. `Settings -> Pull Requests`
   - ensure merge method you want is enabled (recommended: squash merge).

## Recommended maintainer routine

1. Merge feature PRs into `master`.
2. Wait for/update Release PR.
3. Review Release PR changelog and version.
4. Merge Release PR.
5. Publish notes updates if needed directly in GitHub Release UI (optional).

## Migration notes

- Keep `docs/legacy-news.md` as historical archive, but treat `CHANGELOG.md` as the source of truth for all new releases.
- If release logic must be reset later, update `.release-please-manifest.json` to the desired baseline version.
