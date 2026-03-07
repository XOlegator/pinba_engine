#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"${script_dir}/require_compile_commands.sh"
project_root="$(cd "${script_dir}/.." && pwd)"
compile_db="${project_root}/build/compile_commands.json"

mapfile -t files < <(
  jq -r '.[].file' "${compile_db}" \
    | grep -E "^${project_root}/src/.*\.(cc|cpp|cxx)$" \
    | sort -u
)

if [[ ${#files[@]} -eq 0 ]]; then
  exit 0
fi

clang-tidy -p "${project_root}/build" "${files[@]}"
