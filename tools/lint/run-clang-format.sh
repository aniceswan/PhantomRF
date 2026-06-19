#!/usr/bin/env bash
# =============================================================================
#  PhantomRF — clang-format check
# ----------------------------------------------------------------------------
#  Walks `src/` and `tests/`, runs `clang-format-14 --dry-run --Werror` on
#  every .h / .cpp / .c file and reports any unformatted ones.
#
#  Exit codes:
#    0  every checked file matches the .clang-format style
#    1  at least one file is not formatted (or clang-format missing)
#    2  invocation error (missing toolchain, no .clang-format, etc.)
# =============================================================================

set -euo pipefail

# ---- configuration ---------------------------------------------------------

# Try clang-format-14 first (matches CI), then fall back to a generic
# `clang-format` if the versioned binary is not installed.
CLANG_FORMAT_BIN="${CLANG_FORMAT_BIN:-}"
if [ -z "${CLANG_FORMAT_BIN}" ]; then
    if command -v clang-format-14 >/dev/null 2>&1; then
        CLANG_FORMAT_BIN="clang-format-14"
    elif command -v clang-format-15 >/dev/null 2>&1; then
        CLANG_FORMAT_BIN="clang-format-15"
    elif command -v clang-format-16 >/dev/null 2>&1; then
        CLANG_FORMAT_BIN="clang-format-16"
    elif command -v clang-format-17 >/dev/null 2>&1; then
        CLANG_FORMAT_BIN="clang-format-17"
    elif command -v clang-format-18 >/dev/null 2>&1; then
        CLANG_FORMAT_BIN="clang-format-18"
    elif command -v clang-format >/dev/null 2>&1; then
        CLANG_FORMAT_BIN="clang-format"
    else
        echo "::error::clang-format is not installed. Install clang-format-14 (apt) or newer." >&2
        exit 2
    fi
fi

# Walk these directories (relative to the repo root).
SEARCH_DIRS=("src" "tests")

# Extensions we format.
EXTENSIONS=("h" "hh" "hpp" "c" "cc" "cpp" "cxx")

# Files we deliberately skip.
EXCLUDE_REGEX='(^|/)(build|\.pio|dist|node_modules)/'

# ---- locate the repo root and the .clang-format file ------------------------

# Resolve the script's own directory and walk up to find the .clang-format
# file at the repo root. If we can't find it, we bail with a clear error
# rather than silently falling back to the LLVM default.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

if [ ! -f "${REPO_ROOT}/.clang-format" ]; then
    echo "::error::.clang-format not found at ${REPO_ROOT}. Did you move the script?" >&2
    exit 2
fi

# ---- collect files ---------------------------------------------------------

declare -a files=()

for dir in "${SEARCH_DIRS[@]}"; do
    full_dir="${REPO_ROOT}/${dir}"
    if [ ! -d "${full_dir}" ]; then
        continue
    fi
    # Build a `find` expression that matches any of our extensions and
    # skips the obvious noise directories.
    pattern_args=()
    for ext in "${EXTENSIONS[@]}"; do
        pattern_args+=(-o -name "*.${ext}")
    done
    # Drop the leading -o from the first pattern.
    pattern_args=("${pattern_args[@]:1}")

    while IFS= read -r -d '' f; do
        # Normalise the path so the regex (anchored on /) works on
        # macOS too (where `find` may emit `./...`).
        rel="${f#${REPO_ROOT}/}"
        if [[ "${rel}" =~ ${EXCLUDE_REGEX} ]]; then
            continue
        fi
        files+=("${f}")
    done < <(find "${full_dir}" -type f \( "${pattern_args[@]}" \) -print0)
done

if [ "${#files[@]}" -eq 0 ]; then
    echo "No C/C++ source files found under ${SEARCH_DIRS[*]}; nothing to check."
    exit 0
fi

echo "Running ${CLANG_FORMAT_BIN} on ${#files[@]} file(s)..."

# ---- run -------------------------------------------------------------------

# --dry-run --Werror exits 1 on the first unformatted file; we keep going
# to print *all* offending files in one pass, then return 1 at the end.
fail=0
for f in "${files[@]}"; do
    rel="${f#${REPO_ROOT}/}"
    if ! "${CLANG_FORMAT_BIN}" --dry-run --Werror "${f}" >/dev/null 2>&1; then
        printf '  unformatted: %s\n' "${rel}"
        fail=1
    fi
done

if [ "${fail}" -ne 0 ]; then
    cat <<EOF >&2

::error::One or more files are not formatted correctly.

To fix the formatting locally, run:

    ${CLANG_FORMAT_BIN} -i <file>

or in bulk:

    find src tests -type f \\( -name '*.h' -o -name '*.cpp' -o -name '*.c' \\) \\
        -print0 | xargs -0 ${CLANG_FORMAT_BIN} -i
EOF
    exit 1
fi

echo "All files match ${CLANG_FORMAT_BIN} style."
exit 0
