# Pinba Engine CMake Build

The active build system is CMake. The project targets MySQL 8.0+ and C++23.

## Requirements

- CMake 3.24+.
- GCC 13+ or a Clang version with practical C++23 support.
- MySQL 8.0 client development files.
- MySQL 8.0 server source headers containing `sql/handler.h`.
- Protocol Buffers 3.x.

## Build

```bash
cmake --preset release
cmake --build --preset release
```

Equivalent command without presets:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

## Options

```bash
# Debug build with Pinba debug logging
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DPINBA_DEBUG=ON

# Unit tests, enabled by default
cmake -S . -B build -DPINBA_WITH_TESTS=ON

# Benchmarks
cmake -S . -B build -DPINBA_WITH_BENCHMARKS=ON

# Download pinned MySQL 8.0 source archive if local server headers are missing
cmake -S . -B build -DPINBA_DOWNLOAD_MYSQL_SOURCE=ON
```

## Tests

```bash
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

## Static Analysis

```bash
./scripts/run_clang_tidy.sh
pre-commit run --all-files
```

CMake generates `build/compile_commands.json` by default.
