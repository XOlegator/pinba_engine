#!/usr/bin/env bash
set -euo pipefail

compile_db="build/compile_commands.json"

if [[ ! -f "${compile_db}" ]]; then
  echo "Missing ${compile_db}."
  echo "Generate it before running static analysis:"
  echo "  cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
  exit 1
fi
