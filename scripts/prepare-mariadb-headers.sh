#!/usr/bin/env bash
# Prepare a MariaDB server source tree with the generated headers a storage-engine
# plugin needs (my_config.h, mysql_version.h, mysqld_error.h, ...), so pinba_engine
# can be built with -DPINBA_DB_FLAVOR=mariadb against it. The MariaDB source tarball
# ships only *.in templates for these, so we run MariaDB's own CMake far enough to
# generate them — no full server build is required (see the project's knowledge base:
# concepts/mysql-plugin-abi.md and the ai_rpm_maria.log R1 note).
#
# Usage:
#   scripts/prepare-mariadb-headers.sh <version> <dest-source-root> [tarball-cache-dir]
#
#     <version>            exact MariaDB version, e.g. 11.8.6 (must match the runtime
#                          server so the plugin ABI lines up)
#     <dest-source-root>   directory to hold the extracted source + generated headers;
#                          pass this as -DPINBA_MYSQL_SOURCE_DIR
#     [tarball-cache-dir]  optional dir to cache the downloaded source tarball
#
# Requires: curl, tar, cmake, a C/C++ compiler, and OpenSSL headers (libssl-dev /
# openssl-devel). WITH_SSL=system uses OpenSSL so GnuTLS is not needed.
set -euo pipefail

ver="${1:?usage: prepare-mariadb-headers.sh <version> <dest-source-root> [cache-dir]}"
dest="${2:?usage: prepare-mariadb-headers.sh <version> <dest-source-root> [cache-dir]}"
cache="${3:-${dest}/.cache}"

mkdir -p "$cache"
tarball="${cache}/mariadb-${ver}.tar.gz"
url="https://archive.mariadb.org/mariadb-${ver}/source/mariadb-${ver}.tar.gz"

if [ ! -s "$tarball" ]; then
    echo ">> downloading ${url}" >&2
    curl -fSL --retry 3 -o "$tarball" "$url"
fi

# Extract to dest (flattening the top-level mariadb-<ver>/ dir into dest).
if [ ! -f "${dest}/sql/handler.h" ]; then
    echo ">> extracting into ${dest}" >&2
    mkdir -p "$dest"
    tar -xzf "$tarball" -C "$dest" --strip-components=1
fi

genbuild="${dest}/_gen"
if [ ! -f "${genbuild}/include/mysqld_error.h" ]; then
    echo ">> configuring MariaDB (header generation only)" >&2
    # CMAKE_POLICY_VERSION_MINIMUM=3.5: CMake 4.x (Fedora 44+) rejects the
    # cmake_minimum_required(<3.5) in some bundled MariaDB storage engines
    # (e.g. columnstore in 10.11); this lets the old sub-projects configure.
    # PLUGIN_COLUMNSTORE=NO skips that engine entirely — we only need GenError.
    cmake -S "$dest" -B "$genbuild" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_SSL=system \
        -DPLUGIN_INNOBASE=NO -DPLUGIN_ROCKSDB=NO \
        -DPLUGIN_MROONGA=NO -DPLUGIN_SPIDER=NO \
        -DPLUGIN_COLUMNSTORE=NO \
        -DWITH_UNIT_TESTS=OFF -DWITH_ZLIB=bundled >&2
    echo ">> generating error headers (comp_err)" >&2
    cmake --build "$genbuild" --target GenError --parallel "$(nproc)" >&2
fi

# Stage the generated headers where the pinba CMake looks for them: <source>/builddir/include.
builddir_inc="${dest}/builddir/include"
mkdir -p "$builddir_inc"
cp -f "${genbuild}/include/"*.h "$builddir_inc/"

echo ">> MariaDB headers ready under ${dest}" >&2
echo ">>   generated: $(cd "$builddir_inc" && echo *.h)" >&2
# stdout carries only the source root, so callers can capture it.
echo "$dest"
