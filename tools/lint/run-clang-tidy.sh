#!/usr/bin/env bash
# =============================================================================
#  PhantomRF — clang-tidy check
# ----------------------------------------------------------------------------
#  Runs `clang-tidy-14` on every .cpp / .cc / .c file under src/ using the
#  rules in .clang-tidy. We deliberately skip tests/ (their style is looser
#  and they live in the host test environment) and lib/ (third-party).
#
#  This script is the same as `pio check` minus the PlatformIO overhead,
#  so it can be run from a vanilla developer shell. In CI we use
#  `pio check -e esp32s3-n16r8` because the toolchain matches the build.
#
#  Exit codes:
#    0  no findings
#    1  at least one warning/error was reported
#    2  invocation error
# =============================================================================

set -euo pipefail

# ---- configuration ---------------------------------------------------------

# If a versioned binary is not available we fall back to plain `clang-tidy`.
# We prefer 14 because that's the version PlatformIO ships with the
# espressif32@6.5 platform and is the same version CI uses.
CLANG_TIDY_BIN="${CLANG_TIDY_BIN:-}"
if [ -z "${CLANG_TIDY_BIN}" ]; then
    if command -v clang-tidy-14 >/dev/null 2>&1; then
        CLANG_TIDY_BIN="clang-tidy-14"
    elif command -v clang-tidy-15 >/dev/null 2>&1; then
        CLANG_TIDY_BIN="clang-tidy-15"
    elif command -v clang-tidy-16 >/dev/null 2>&1; then
        CLANG_TIDY_BIN="clang-tidy-16"
    elif command -v clang-tidy >/dev/null 2>&1; then
        CLANG_TIDY_BIN="clang-tidy"
    else
        echo "::error::clang-tidy is not installed. Install clang-tidy-14 (apt) or newer." >&2
        exit 2
    fi
fi

SEARCH_DIRS=("src")
EXCLUDE_REGEX='(^|/)(build|\.pio|dist|tests|lib)/'
EXTENSIONS=("cpp" "cc" "cxx" "c")

# ---- locate repo root -------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

if [ ! -f "${REPO_ROOT}/.clang-tidy" ]; then
    echo "::error::.clang-tidy not found at ${REPO_ROOT}." >&2
    exit 2
fi

# ---- collect files ---------------------------------------------------------

declare -a files=()

for dir in "${SEARCH_DIRS[@]}"; do
    full_dir="${REPO_ROOT}/${dir}"
    [ -d "${full_dir}" ] || continue

    pattern_args=()
    for ext in "${EXTENSIONS[@]}"; do
        pattern_args+=(-o -name "*.${ext}")
    done
    pattern_args=("${pattern_args[@]:1}")

    while IFS= read -r -d '' f; do
        rel="${f#${REPO_ROOT}/}"
        if [[ "${rel}" =~ ${EXCLUDE_REGEX} ]]; then
            continue
        fi
        # Skip subdirectories that are clearly third-party.
        case "${rel}" in
            */lib/*|*/vendor/*|*/third_party/*) continue ;;
        esac
        files+=("${f}")
    done < <(find "${full_dir}" -type f \( "${pattern_args[@]}" \) -print0)
done

if [ "${#files[@]}" -eq 0 ]; then
    echo "No C/C++ source files found under ${SEARCH_DIRS[*]}; nothing to check."
    exit 0
fi

# ---- command line ---------------------------------------------------------

# Extra args that get appended to the clang-tidy invocation. You can extend
# this with extra -checks= or -header-filter= arguments if you want to
# debug a specific category.
TIDY_ARGS=(
    "--quiet"
    "--use-color"
    "--warnings-as-errors=*"
    # `--header-filter` mirrors HeaderFilterRegex in .clang-tidy so the
    # diagnostics don't get drowned in system header noise.
    "--header-filter=src/.*"
)

echo "Running ${CLANG_TIDY_BIN} on ${#files[@]} file(s)..."

# ---- run -------------------------------------------------------------------

# We collect exit codes per file so that one file's failure does not
# short-circuit the rest of the run.
fail=0
for f in "${files[@]}"; do
    rel="${f#${REPO_ROOT}/}"
    # `--` ensures clang-tidy treats the path as a positional filename,
    # not a flag.
    if ! "${CLANG_TIDY_BIN}" "${TIDY_ARGS[@]}" -- "${f}" 2>/dev/null; then
        printf '  issues: %s\n' "${rel}"
        fail=1
    fi
done

if [ "${fail}" -ne 0 ]; then
    cat <<EOF >&2

::error::clang-tidy reported issues. See the lines above for the exact
filename:line:column locations.

To inspect a specific file in detail, run:

    ${CLANG_TIDY_BIN} ${TIDY_ARGS[*]} -- ${REPO_ROOT}/src/path/to/file.cpp
EOF
    exit 1
fi

echo "clang-tidy is happy with src/."
exit 0
