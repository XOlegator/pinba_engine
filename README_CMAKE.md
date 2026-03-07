# Pinba Engine - CMake Build System

## Overview

This document describes the CMake-based build system for Pinba Engine, targeting MySQL 8.0+ and modern C++ standards.

## Requirements

- CMake 3.16+
- GCC 9+ or Clang 10+ (C++17)
- MySQL 8.0+ development packages
- Protocol Buffers 3.0+

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
sudo make install
```

## Common build options

```bash
# Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DPINBA_DEBUG=ON

# Static build
cmake .. -DPINBA_STATIC_BUILD=ON

# Build tests
cmake .. -DPINBA_WITH_TESTS=ON

# Build benchmarks
cmake .. -DPINBA_WITH_BENCHMARKS=ON
```

## Tests

```bash
cmake .. -DPINBA_WITH_TESTS=ON
make -j"$(nproc)"
ctest --output-on-failure
```

## Migration note

Legacy autotools files were kept only for historical context in older branches.
The active development path for this fork is CMake.
