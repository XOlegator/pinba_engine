---
title: "Minimal MySQL Vendor Headers Strategy"
type: concept
sources:
  - raw/sessions/ppa-packaging-session-2026-06-04.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/cmake-build-system.md
  - wiki/concepts/mysql-plugin-abi.md
confidence: high
updated: 2026-06-07
---

# Minimal MySQL Vendor Headers Strategy

A MySQL storage engine plugin requires headers from the full MySQL source tree
(e.g., `sql/handler.h`) that are not present in `libmysqlclient-dev`.

For offline builds (Launchpad PPA, CI without network access) these headers must be
vendored. The strategy is to store **only the files that the compiler actually includes**.

---

## The problem with the naive approach

A simple `tar -xf mysql.tar.gz mysql-8.0.46/include/ mysql-8.0.46/sql/ ...` extracts
roughly 1 300 files (~14 MB) per series. The majority are MySQL internals (Boost patches,
GIS, join optimizer, data dictionary implementation, range optimizer, ...) that
`pinba_engine` never includes.

**Actual numbers:**

| | Before | After |
|---|---|---|
| Files (8.0) | 1317 | 162 |
| Files (8.4) | ~1317 | 175 |
| Size per series | ~14 MB | ~2.2 MB |
| Both series in git | ~28 MB | ~4.5 MB |

---

## How to obtain the minimal file list

The compiler records every file it actually includes in `.d` files (Makefile dependency
files). CMake generates these automatically.

### Step 1: run a build

```bash
cmake -B build-analyze \
    -DPINBA_MYSQL_SOURCE_DIR=vendor/mysql-headers-8.0 \
    -DPINBA_MYSQL_SOURCE_VERSION=8.0.46 \
    -DPINBA_WITH_TESTS=OFF
cmake --build build-analyze
```

### Step 2: extract the file list from .d files

```python
import re, glob

vendor_dir = '/path/to/vendor/mysql-headers-8.0'
pattern = rf'{re.escape(vendor_dir)}/([^\s\\\\]+)'
used = set()
for f in glob.glob('build-analyze/CMakeFiles/pinba_engine.dir/**/*.d', recursive=True):
    for line in open(f):
        for m in re.findall(pattern, line):
            p = m.strip()
            if not p.startswith('builddir/'):  # builddir/ is cmake-generated, not from the tarball
                used.add(p)
print(f'{len(used)} files actually used')
```

### Step 3: merge across all series

Some paths differ between series:
- MySQL 8.0: `libbinlogevents/export/binary_log_funcs.h`
- MySQL 8.4: `libs/mysql/binlog/event/export/binary_log_funcs.h`

Take the union of both sets — files absent from a specific tarball are simply skipped
during extraction.

---

## Algorithm: extract-mysql-headers.sh

```bash
# 1. Download tarball (~300 MB)
wget "$URL" -O "$TMPDIR/mysql.tar.gz"

# 2. List the archive contents (one pass, reads only tar headers)
tar -tf "$TMPDIR/mysql.tar.gz" > "$TMPDIR/archive-index.txt"

# 3. Intersect with whitelist
for rel in $NEEDED_FILES; do
    tarpath="mysql-${VERSION}/${rel}"
    if grep -qF "$tarpath" "$TMPDIR/archive-index.txt"; then
        echo "$tarpath" >> "$TMPDIR/to-extract.txt"
    fi
done

# 4. Extract only the needed files in a single pass
tar -xf "$TMPDIR/mysql.tar.gz" \
    --strip-components=1 -C "$DEST" \
    -T "$TMPDIR/to-extract.txt"
```

Two passes over the tarball: one for the index, one for extraction. This is equivalent to
"extract everything, then delete unwanted files" but more explicit and produces no
intermediate junk.

---

## What is NOT needed (removed)

| Directory | Contents | Reason removed |
|-----------|----------|----------------|
| `include/boost_1_77_0/` | Boost geometry patches | MySQL internal, not used by pinba |
| `sql/gis/` (except srid.h) | GIS functions | MySQL internal |
| `sql/join_optimizer/` | JOIN optimizer | MySQL internal |
| `sql/range_optimizer/` | Range scan optimizer | MySQL internal |
| `sql/dd/impl/` | Data Dictionary implementation | Only types needed (`dd/types/`) |
| `sql/auth/` (except auth_acls.h) | Authentication internals | Only ACLs needed |
| `sql/iterators/` | Row iterators | MySQL internal |
| `sql/server_component/` | Server component implementation | Only bits needed |
| `include/authentication_kerberos*` | Kerberos client options | Not needed by the plugin |

---

## What IS needed (162 files for 8.0)

**include/** (111 files):
- `my_*.h` — basic MySQL types and utilities
- `mysql.h`, `mysql_com.h`, `mysql_time.h` — client API
- `mysql/plugin.h` — Plugin API (the main header)
- `mysql/psi/*.h` — Performance Schema instrumentation
- `mysql/components/services/bits/*.h` — bit types for components
- `mysql/service_*.h` — services for plugins

**sql/** (46 files):
- `handler.h`, `table.h`, `field.h` — core storage engine API
- `key.h`, `mdl.h`, `sql_plugin.h` — handler.h dependencies
- `dd/types/*.h` — Data Dictionary types
- `gis/srid.h` — only this one file from gis/

**libbinlogevents/** (3 files, MySQL 8.0 only):
- `export/binary_log_funcs.h`
- `include/table_id.h`
- `include/wrapper_functions.h`

**libs/mysql/binlog/** (3 files, MySQL 8.4 only — replaces libbinlogevents/):
- Same three files, different path

---

## Updating the whitelist after code changes

If `pinba_engine` gains a new `#include <sql/some_new.h>`:

1. Run a build: `cmake --build build`
2. Run the Python script above for both 8.0 and 8.4
3. Add new files to `NEEDED_FILES` in `tools/extract-mysql-headers.sh`
4. Re-run `tools/extract-mysql-headers.sh 8.0 8.0.XX` and `8.4 8.4.XX`
5. Commit the updated `vendor/mysql-headers-*/`
