# Releases (semi-automated) on GitHub

## Goal

This project uses a semi-automated release flow on GitHub:

- version bump is calculated from Conventional Commits;
- release PR is created automatically;
- `CHANGELOG.md` is updated automatically;
- Git tag and GitHub Release are created automatically after release PR merge.

This replaces the old manual `NEWS`-driven flow and is intended for active development in the fork.

## What is automated

Configured via GitHub Actions + Release Please:

- open/update a **Release PR** to `master`;
- calculate next SemVer version:
  - `major`: commit with `!` or `BREAKING CHANGE:`;
  - `minor`: at least one `feat`;
  - `patch`: everything else (`fix`, `perf`, `refactor`, `docs`, `test`, `chore`);
- update `CHANGELOG.md`;
- create a Git tag (`vX.Y.Z`) and GitHub Release after merge.

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
2. GitHub Action `release-please` opens/updates a Release PR.
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

- Keep `NEWS` as historical archive if needed, but treat `CHANGELOG.md` as the source of truth for all new releases.
- If release logic must be reset later, update `.release-please-manifest.json` to the desired baseline version.

