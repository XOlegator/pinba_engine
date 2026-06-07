---
title: "PHP Extension Build System"
type: concept
sources:
  - raw/repos/php-pinba-extension.md
related:
  - wiki/sources/pinba-extension-fork.md
  - wiki/concepts/php-extension-api.md
  - wiki/concepts/cmake-build-system.md
confidence: high
updated: 2026-06-07
---

# PHP Extension Build System

The PHP extension uses `phpize` + autoconf — different from the MySQL plugin's CMake build.

## Source Files

Three C files compiled into `pinba.so`:

| File | Role |
|------|------|
| `pinba.c` | PHP extension: function registration, INI directives, request lifecycle hooks |
| `pinba.pb-c.c` | Generated protobuf-c serializer for `Pinba__Request` |
| `protobuf-c.c` | Bundled protobuf-c runtime |

All compiled with `-DNDEBUG`. `protobuf-c` is bundled so the extension has no runtime
dependency on `libprotobuf-c` (unlike the MySQL plugin which requires `libprotobuf.so`).

## Build Steps

```bash
phpize                          # generate configure script from config.m4
./configure --enable-pinba      # configure for the active PHP installation
make                            # compile pinba.so
sudo make install               # install to PHP's extension_dir
make test TESTS=tests/          # run PHPT test suite
```

`phpize` reads `config.m4` and generates the autoconf `configure` script for the currently
active PHP installation. To build for a non-default PHP version, call
`phpize` from the correct PHP's binary path:

```bash
/usr/bin/php8.4 /usr/bin/phpize8.4
./configure --with-php-config=/usr/bin/php-config8.4
make
```

## config.m4

The build config file (`config.m4`) handles:

```m4
PHP_ARG_ENABLE([pinba], [whether to enable pinba support],
  [--enable-pinba   Enable pinba support], [no])

if test "$PHP_PINBA" != "no"; then
  PHP_CHECK_FUNC(mallinfo, unistd.h)   # glibc mallinfo() availability
  PHP_NEW_EXTENSION(pinba, pinba.c pinba.pb-c.c protobuf-c.c, $ext_shared, ,, ,no)
fi
```

`PHP_CHECK_FUNC(mallinfo)` — tests for the deprecated `mallinfo()` function.
The fork uses `mallinfo2()` on glibc 2.33+; the config.m4 check guards the fallback.

## PHPT Test Format

PHPT is PHP's native test format. Each `.phpt` file contains sections:

```
--TEST--
Short description of what is tested
--EXTENSIONS--
pinba
--INI--
pinba.enabled=0
--FILE--
<?php
// test code
?>
--EXPECT--
expected output (exact match)
```

Sections relevant to this extension:

| Section | Use |
|---------|-----|
| `--EXTENSIONS--` | Ensures `pinba` is loaded before the test runs |
| `--INI--` | Overrides `php.ini` settings for the test |
| `--SKIPIF--` | Skip condition (e.g., extension not available) |
| `--FILE--` | PHP code executed in a subprocess |
| `--EXPECT--` | Exact expected stdout |
| `--EXPECTF--` | Expected stdout with `%s`, `%d` printf-style wildcards |

On failure, `make test` generates `.diff`, `.exp`, `.log`, and `.out` files in `tests/`
which CI collects as artifacts.

## Multi-PHP Build Matrix

The CI matrix builds across all currently supported PHP branches. The list of branches
is not hardcoded in `ci.yml` — it is read from `.github/php-versions.json`.

```yaml
# ci.yml (simplified)
jobs:
  matrix:
    outputs:
      php-versions: ${{ steps.read-matrix.outputs.php-versions }}
    steps:
      - run: echo "php-versions=$(jq -c '.targets.php' .github/php-versions.json)" >> "$GITHUB_OUTPUT"

  build-and-test:
    strategy:
      matrix:
        php-version: ${{ fromJson(needs.matrix.outputs.php-versions) }}
    steps:
      - uses: shivammathur/setup-php@v2
        with:
          php-version: ${{ matrix.php-version }}
          tools: phpize
      - run: phpize && ./configure --enable-pinba && make
      - run: make test TESTS=tests/
```

`shivammathur/setup-php@v2` installs any PHP version on `ubuntu-latest` runners,
including `phpize` and `php-config`. No system PHP installation is needed.

## Contrast with MySQL Plugin Build

| Aspect | PHP extension | MySQL plugin |
|--------|--------------|--------------|
| Build system | phpize + autoconf | CMake |
| Config | config.m4 | CMakeLists.txt |
| Compiler flags | From `phpize` environment | Explicit in CMakeLists |
| Link target | PHP's ABI | MySQL's ABI |
| Versioning | php_pinba.h constant | CMake `project(VERSION ...)` |
| CI variation | PHP version matrix | MySQL version matrix |

Both extensions use a CI matrix to verify compatibility across multiple target versions.
See [[cmake-build-system]] for the MySQL plugin build.

## Local Build for a Specific PHP Version (Ubuntu)

```bash
sudo apt install php8.4-dev
git clone https://github.com/XOlegator/pinba_extension
cd pinba_extension
phpize8.4
./configure --with-php-config=/usr/bin/php-config8.4
make
make test TESTS=tests/
sudo make install
```

After install, add to `/etc/php/8.4/mods-available/pinba.ini`:

```ini
extension=pinba.so
```

Then: `sudo phpenmod -v 8.4 pinba`

See: [[php-extension-api]], [[php-extension-packaging]], [[php-version-monitoring]]
