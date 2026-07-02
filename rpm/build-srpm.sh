#!/usr/bin/env bash
# Build the pinba-engine source RPM (and optionally the binary RPMs) from a clean
# checkout, so local runs and CI share one recipe (mirrors the Debian track's
# build logic living in scripts, not inline YAML).
#
#   rpm/build-srpm.sh <version> [--rpms]
#
#     <version>   upstream version to stamp into the spec + tarball, e.g. 2.7.0
#     --rpms      also build the binary RPMs (needs the BuildRequires from the
#                 spec, all in base Fedora/EL); without it only the .src.rpm is
#                 produced
#
# Unlike the PHP extension, the engine is a storage-engine plugin that compiles
# against MySQL *server* source headers. Those headers are vendored in-tree under
# vendor/mysql-headers-8.x and travel inside the source tarball, so the RPM builds
# fully offline in mock/Copr — no network and no server package at build time.
#
# Requires (Fedora/EL): rpm-build, git, tar, gzip; with --rpms also dnf-plugins-core.
set -euo pipefail

ver="${1:?usage: build-srpm.sh <version> [--rpms]}"
mode="${2:-}"
root="$(cd "$(dirname "$0")/.." && pwd)"

top="$(mktemp -d)"
mkdir -p "$top"/{SOURCES,SPECS,BUILD,RPMS,SRPMS}

# Stamp the requested version into a working copy of the spec.
sed "s/^Version:.*/Version:        ${ver}/" "$root/rpm/pinba-engine.spec" > "$top/SPECS/pinba-engine.spec"

# Pristine source tarball, unpacking to pinba-engine-<ver>/ as %autosetup expects.
# git archive includes the vendored server headers (vendor/mysql-headers-8.x).
git -c safe.directory='*' -C "$root" archive --prefix="pinba-engine-${ver}/" HEAD \
    | gzip -9 > "$top/SOURCES/pinba-engine-${ver}.tar.gz"

# Build noise goes to stderr so stdout carries only the artifact paths, which
# callers (the publish workflow) parse to find the .src.rpm.
if [ "$mode" = "--rpms" ]; then
    dnf -y builddep "$top/SPECS/pinba-engine.spec" >&2
    rpmbuild --define "_topdir $top" -ba "$top/SPECS/pinba-engine.spec" >&2
else
    rpmbuild --define "_topdir $top" -bs "$top/SPECS/pinba-engine.spec" >&2
fi

echo "TOPDIR=$top" >&2
find "$top/SRPMS" "$top/RPMS" -name '*.rpm' 2>/dev/null | sort
