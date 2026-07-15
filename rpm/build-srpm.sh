#!/usr/bin/env bash
# Build the pinba-engine source RPM (and optionally the binary RPMs) from a clean
# checkout, so local runs and CI share one recipe (mirrors the Debian track's
# build logic living in scripts, not inline YAML).
#
#   rpm/build-srpm.sh <version> [mysql|mariadb] [--rpms]
#
#     <version>          upstream version to stamp into the spec + tarball, e.g. 2.7.0
#     mysql | mariadb    flavor to build (default: mysql). Baked into the SRPM so a
#                        Copr rebuild produces the right flavor.
#     --rpms             also build the binary RPMs (needs the BuildRequires from the
#                        spec, all in base Fedora/EL); without it only the .src.rpm
#                        is produced
#
# Unlike the PHP extension, the engine is a storage-engine plugin that compiles
# against MySQL/MariaDB *server* source headers. Those headers are vendored in-tree
# (vendor/mysql-headers-8.x, vendor/mariadb-headers-*) and travel inside the source
# tarball, so the RPM builds fully offline in mock/Copr — no network and no server
# package at build time.
#
# Requires (Fedora/EL): rpm-build, git, tar, gzip; with --rpms also dnf-plugins-core.
set -euo pipefail

ver="${1:?usage: build-srpm.sh <version> [mysql|mariadb] [--rpms]}"
shift || true

flavor="mysql"
rpms=0
for arg in "$@"; do
    case "$arg" in
        mysql|mariadb) flavor="$arg" ;;
        --rpms) rpms=1 ;;
        *) echo "unknown argument: $arg" >&2; exit 2 ;;
    esac
done

root="$(cd "$(dirname "$0")/.." && pwd)"

top="$(mktemp -d)"
mkdir -p "$top"/{SOURCES,SPECS,BUILD,RPMS,SRPMS}

# Stamp the version into a working copy of the spec, and bake the flavor in (as a
# leading %global, which the spec's %{!?flavor:...} default then leaves alone) so
# the SRPM rebuilds to the same flavor on Copr regardless of any --define.
spec="$top/SPECS/pinba-engine.spec"
{
    printf '%%global flavor %s\n' "$flavor"
    sed "s/^Version:.*/Version:        ${ver}/" "$root/rpm/pinba-engine.spec"
} > "$spec"

# Pristine source tarball, unpacking to pinba-engine-<ver>/ as %autosetup expects.
# git archive includes the vendored server headers (vendor/*-headers-*).
git -c safe.directory='*' -C "$root" archive --prefix="pinba-engine-${ver}/" HEAD \
    | gzip -9 > "$top/SOURCES/pinba-engine-${ver}.tar.gz"

# Build noise goes to stderr so stdout carries only the artifact paths, which
# callers (the publish workflow) parse to find the .src.rpm.
if [ "$rpms" = 1 ]; then
    dnf -y builddep "$spec" >&2
    rpmbuild --define "_topdir $top" -ba "$spec" >&2
else
    rpmbuild --define "_topdir $top" -bs "$spec" >&2
fi

echo "TOPDIR=$top" >&2
find "$top/SRPMS" "$top/RPMS" -name '*.rpm' 2>/dev/null | sort
