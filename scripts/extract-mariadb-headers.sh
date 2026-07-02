#!/usr/bin/env bash
# Extract the minimal subset of MariaDB server headers needed to build ha_pinba.so
# for the MariaDB flavor, so the RPM/mock build is fully offline (no 120 MB source
# download + CMake header-generation at package build time). Mirrors
# tools/extract-mysql-headers.sh, but derives the file list dynamically from the
# compiler .d dependency files of a real build and then *validates* the subset by
# rebuilding the plugin against only the extracted tree.
#
# Usage:
#   scripts/extract-mariadb-headers.sh <version> <series> [dest]
#     <version>  exact MariaDB version, e.g. 11.8.6
#     <series>   series label used for the vendor dir, e.g. 11.8
#     [dest]     output vendor dir (default: vendor/mariadb-headers-<series>)
#
# Requires a full pinba build toolchain + MariaDB header-gen deps (cmake, gcc-c++,
# bison, openssl-devel, protobuf, rapidjson, curl, tar). Intended to run in a
# container/dev env; commit the resulting vendor/mariadb-headers-<series> tree.
set -euo pipefail

ver="${1:?usage: extract-mariadb-headers.sh <version> <series> [dest]}"
series="${2:?usage: extract-mariadb-headers.sh <version> <series> [dest]}"
root="$(cd "$(dirname "$0")/.." && pwd)"
dest="${3:-$root/vendor/mariadb-headers-$series}"

work="$(mktemp -d)"
mdb="$work/mariadb-src"
bld="$work/build"
cache="${MDB_CACHE:-$work/cache}"

# 1. Full MariaDB source tree with generated headers (my_config.h, mysqld_error.h…).
bash "$root/scripts/prepare-mariadb-headers.sh" "$ver" "$mdb" "$cache" >&2

# 2. Build the MariaDB-flavor plugin so the compiler emits .d dependency files.
cmake -B "$bld" -S "$root" \
    -DPINBA_DB_FLAVOR=mariadb \
    -DPINBA_MYSQL_SERIES="$series" \
    -DPINBA_MYSQL_SOURCE_DIR="$mdb" \
    -DPINBA_MYSQL_SOURCE_VERSION="$ver" \
    -DMYSQL_INCLUDE_DIR="$mdb/include" \
    -DPINBA_WITH_TESTS=OFF \
    -DPINBA_FETCH_DEPENDENCIES=OFF >&2
cmake --build "$bld" --parallel "$(nproc)" --target pinba_engine >&2

# 3. Union of every header the build actually read from the MariaDB source tree,
#    taken from the .d files. Always include the generated header set (referenced
#    via a build-dir copy, so it may not appear under $mdb in the .d).
mapfile -t used < <(
    find "$bld" -name '*.d' -exec cat {} + 2>/dev/null \
    | tr ' \\' '\n\n' | grep "^$mdb/" | sed "s|^$mdb/||" | sort -u
)
forced=(
    builddir/include/my_config.h
    builddir/include/mysql_version.h
    builddir/include/mysqld_error.h
    builddir/include/mysqld_ername.h
    builddir/include/probes_mysql_nodtrace.h
    builddir/include/probes_mysql.h
)

# 4. Stage the minimal subset, preserving the tree layout.
rm -rf "$dest"
mkdir -p "$dest"
for f in "${used[@]}" "${forced[@]}"; do
    if [ -f "$mdb/$f" ]; then
        install -Dm 0644 "$mdb/$f" "$dest/$f"
    fi
done
echo "$ver" > "$dest/.mariadb-version"

# 5. Validate: rebuild the plugin against ONLY the extracted subset (offline shape).
val="$work/build-validate"
cmake -B "$val" -S "$root" \
    -DPINBA_DB_FLAVOR=mariadb \
    -DPINBA_MYSQL_SERIES="$series" \
    -DPINBA_MYSQL_SOURCE_DIR="$dest" \
    -DPINBA_MYSQL_SOURCE_VERSION="$ver" \
    -DMYSQL_INCLUDE_DIR="$dest/include" \
    -DPINBA_WITH_TESTS=OFF \
    -DPINBA_FETCH_DEPENDENCIES=OFF >&2
cmake --build "$val" --parallel "$(nproc)" --target pinba_engine >&2

count=$(find "$dest" -type f | wc -l)
size=$(du -sh "$dest" | cut -f1)
echo ">> validated: ha_pinba.so builds against the vendored subset ($count files, $size)" >&2
echo "$dest"
