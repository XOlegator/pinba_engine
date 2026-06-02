#!/usr/bin/env bash
#
# Extract the MySQL server plugin headers needed to build ha_pinba.so.
#
# Usage: tools/extract-mysql-headers.sh SERIES VERSION
#   SERIES  — major.minor, e.g. 8.0 or 8.4
#   VERSION — full version, e.g. 8.0.46 or 8.4.9
#
# Result: vendor/mysql-headers-SERIES/ (~15 MB) committed to git.
# Update this directory whenever the MySQL patch version changes.
#
# Example:
#   bash tools/extract-mysql-headers.sh 8.0 8.0.46
#   bash tools/extract-mysql-headers.sh 8.4 8.4.9
#   git add vendor/mysql-headers-8.0 vendor/mysql-headers-8.4
#   git commit -m "chore: update MySQL headers 8.0.46 / 8.4.9"
set -euo pipefail

SERIES="${1:-}"
VERSION="${2:-}"

if [ -z "$SERIES" ] || [ -z "$VERSION" ]; then
    echo "Usage: $0 SERIES VERSION  (e.g. $0 8.0 8.0.46)" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST="$REPO_ROOT/vendor/mysql-headers-$SERIES"
URL="https://dev.mysql.com/get/Downloads/MySQL-${SERIES}/mysql-${VERSION}.tar.gz"

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

echo "==> Downloading MySQL ${VERSION} source..."
wget --quiet --show-progress "$URL" -O "$TMPDIR/mysql.tar.gz"

echo "==> Extracting headers to $DEST ..."
rm -rf "$DEST"
mkdir -p "$DEST"

# These directories from the MySQL source tree are referenced by CMakeLists.txt
# as include paths via PINBA_MYSQL_SOURCE_DIR:
#   ${MYSQL_SQL_INCLUDE_DIR}           → vendor/mysql-headers-X.Y/
#   ${MYSQL_SQL_INCLUDE_DIR}/include   → vendor/mysql-headers-X.Y/include/
#   ${MYSQL_SQL_INCLUDE_DIR}/sql       → vendor/mysql-headers-X.Y/sql/
#   ${MYSQL_SQL_INCLUDE_DIR}/libs      → vendor/mysql-headers-X.Y/libs/
# libbinlogevents/ is included transitively from sql/log_event.h and friends.

for subdir in include sql libbinlogevents libs; do
    if tar -tf "$TMPDIR/mysql.tar.gz" "mysql-${VERSION}/${subdir}/" \
            >/dev/null 2>&1; then
        tar -xf "$TMPDIR/mysql.tar.gz" \
            --strip-components=1 \
            -C "$DEST" \
            "mysql-${VERSION}/${subdir}"
        echo "    extracted: ${subdir}/"
    else
        echo "    skipped (not in tarball): ${subdir}/"
    fi
done

# Keep only header files in directories that also contain source files,
# to avoid shipping unnecessary .cc/.cpp code.
for subdir in sql libbinlogevents libs; do
    if [ -d "$DEST/$subdir" ]; then
        find "$DEST/$subdir" -type f \
            ! \( -name "*.h" -o -name "*.inc" -o -name "*.i" \) \
            -delete
    fi
done

# Record the exact MySQL version for reference.
echo "$VERSION" > "$DEST/.mysql-version"

echo "==> Done."
du -sh "$DEST"
echo ""
echo "Next steps:"
echo "  git add vendor/mysql-headers-${SERIES}"
echo "  git commit -m \"chore: update MySQL ${SERIES} headers to ${VERSION}\""
