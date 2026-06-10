---
title: "AppStream Metainfo Packaging (Author & License in Software Centres)"
type: concept
sources:
  - wiki/sources/test-infra-appstream-session-2026-06-10.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/github-actions-ppa.md
confidence: high
updated: 2026-06-10
---

# AppStream Metainfo Packaging

How the Pinba Engine Debian packages get a proper "Author" and "License" in graphical
software centres (KDE Discover, GNOME Software). This is separate from `debian/control` and
`debian/copyright`.

## Where software centres read Author / License

Discover and GNOME Software do **not** show Author/License from `debian/control`
(`Maintainer:`) or `debian/copyright`. They read it from the **AppStream component** declared
in a metainfo XML file installed at `/usr/share/metainfo/<id>.metainfo.xml`. The relevant
fields are `<developer><name>` (Author) and `<project_license>` (License).

## The critical element: <pkgname>

A metainfo component is bound to an installed binary package by the `<pkgname>` element.
**Without `<pkgname>`, the component is not associated with any package**, so the package's
card in Discover falls back to PackageKit data and shows "Author unknown" / "license unknown"
— even though the metainfo file is installed and `appstreamcli validate` passes.

Diagnosis: convert the metainfo to the DEP-11 catalog that software centres consume:

```
appstreamcli convert debian/<pkg>.metainfo.xml out.yml
```

A correct component shows `Package:`, `ProjectLicense:` and `Developer:` lines. A missing
`Package:` line is the symptom.

## One metainfo per binary package

Each binary package that should carry metadata needs its own metainfo with a unique
component `<id>` and a `<pkgname>` pointing at that package. For the engine:

- `pinba-engine-common` — the main `generic` component.
- `pinba-engine-mysql-8.0` / `-8.4` — `type="addon"` components that `<extends>` the common
  one, each with its own `<id>`, `<pkgname>`, `<developer>` and `<project_license>`.

`debian/rules` installs each file as `/usr/share/metainfo/<id>.metainfo.xml`, gated by the
build's `ENABLE_MYSQL80/84` flags (each Ubuntu series ships only its MySQL series — see
[[github-actions-ppa]]).

## Validation gotcha: hyphens in the rDNS id

`appstreamcli validate` warns `cid-rdns-contains-hyphen` when a hyphen appears in a
non-final segment of a reverse-DNS component id. Keep the hyphenated token as the final
segment, e.g. `io.github.xolegator.pinba-engine-mysql80` (mirroring the common id
`io.github.xolegator.pinba-engine`), not `io.github.xolegator.pinba-engine.mysql80`. The CI
runs `appstreamcli validate`, which fails on warnings, so this must be clean.

## Releases kept current automatically

The `<releases>` list goes stale if bumped by hand. The PPA build workflow injects the
version being packaged as the newest `<release>` (idempotently) into every
`debian/*.metainfo.xml` before building, and validates each file. See [[github-actions-ppa]].

## How the metadata reaches users

Metainfo updates are published with the package: on the next PPA rebuild/release the new
metainfo ships in the `.deb`, AppStream's local pool picks it up at install time, and the
Author/License appear in the software centre. `appstreamcli refresh --force` forces a local
cache refresh. See [[debian-ppa-packaging]] for the overall packaging flow.
