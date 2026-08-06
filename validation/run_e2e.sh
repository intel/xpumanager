#!/usr/bin/env bash
# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT
#
# run_e2e.sh — End-to-end test runner for xpu-smi
#
# Runs all YAML test suites in the suite directory via run_tests.py.
# Captures both stdout and stderr for every command under test.
# Writes a per-issue report file for every test failure.
#
# Usage:
#   ./run_e2e.sh [options]
#
# Options:
#   -b <binary>     Path to xpu-smi binary
#                   (default: ../builddir/ial/cli/xpu-smi relative to this script,
#                    or the value of the XPU_SMI env variable)
#   -s <dir>        Directory holding the YAML test suites
#                   (default: auto-detected — the one subdirectory of this
#                    script's directory that contains *.yaml suites)
#   -o <dir>        Output directory for logs and issue files
#                   (default: e2e_results_<timestamp>)
#   -j <N>          Run up to N tests in parallel (default: 1)
#   -t <tags>       Comma-separated tags to run (e.g. sanity,error)
#   -v              Verbose: show stdout/stderr of each test command
#   -h              Show this help message
#
# Exit codes:
#   0  All tests passed
#   1  One or more tests failed (issue files written to <output-dir>/issues/)
#   2  Setup error (missing binary, Python, or dependencies)
#
# Environment variables:
#   XPU_SMI          Path to the xpu-smi binary (overridden by -b)
#   XPU_SMI_SUITES   Directory holding the YAML suites (overridden by -s)
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ---- Defaults ---------------------------------------------------------------
DEFAULT_BINARY="${XPU_SMI:-${SCRIPT_DIR}/../builddir/ial/cli/xpu-smi}"
BINARY=""
SUITES_DIR="${XPU_SMI_SUITES:-}"
OUTPUT_DIR=""
PARALLEL=1
TAGS=""
VERBOSE=false

# ---- Helpers ----------------------------------------------------------------
usage() {
    sed -n '/^# Usage:/,/^# ====/p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'
    exit 0
}

die() { echo "ERROR: $*" >&2; exit 2; }

log() { echo "[run_e2e] $*"; }

# ---- Argument parsing -------------------------------------------------------
while getopts "b:s:o:j:t:vh" opt; do
    case $opt in
        b) BINARY="$OPTARG" ;;
        s) SUITES_DIR="$OPTARG" ;;
        o) OUTPUT_DIR="$OPTARG" ;;
        j) PARALLEL="$OPTARG" ;;
        t) TAGS="$OPTARG" ;;
        v) VERBOSE=true ;;
        h) usage ;;
        *) die "Unknown option: -$opt  (use -h for help)" ;;
    esac
done

# ---- Resolve binary ---------------------------------------------------------
BINARY="${BINARY:-$DEFAULT_BINARY}"
BINARY="$(realpath -m "$BINARY" 2>/dev/null || echo "$BINARY")"

if [[ ! -x "$BINARY" ]]; then
    die "xpu-smi binary not found or not executable: $BINARY
Specify the path with -b or set the XPU_SMI environment variable."
fi

log "Binary: $BINARY"

# ---- Resolve output directory -----------------------------------------------
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
OUTPUT_DIR="${OUTPUT_DIR:-${SCRIPT_DIR}/e2e_results_${TIMESTAMP}}"
ISSUES_DIR="${OUTPUT_DIR}/issues"
LOG_FILE="${OUTPUT_DIR}/run_e2e_${TIMESTAMP}.log"
JUNIT_XML="${OUTPUT_DIR}/junit.xml"
SUMMARY_JSON="${OUTPUT_DIR}/summary.json"

mkdir -p "$OUTPUT_DIR" "$ISSUES_DIR"
log "Output directory: $OUTPUT_DIR"
log "Issue files:      $ISSUES_DIR"
log "Log file:         $LOG_FILE"
log "JUnit XML:        $JUNIT_XML"
log "Summary JSON:     $SUMMARY_JSON"

# ---- Check Python -----------------------------------------------------------
PYTHON=""
for py in python3 python; do
    if command -v "$py" &>/dev/null; then
        PYTHON="$py"
        break
    fi
done
[[ -n "$PYTHON" ]] || die "Python 3 not found. Please install Python 3."

PY_VERSION="$("$PYTHON" -c 'import sys; print(sys.version_info[:2])')"
log "Python: $PYTHON ($PY_VERSION)"

# ---- Install dependencies ---------------------------------------------------
REQUIREMENTS="${SCRIPT_DIR}/requirements.txt"
if [[ -f "$REQUIREMENTS" ]]; then
    log "Checking Python dependencies from $REQUIREMENTS ..."
    # Try pip install; if the system Python is externally managed, add --break-system-packages
    # as a fallback.  Running inside a virtualenv never needs that flag.
    if ! "$PYTHON" -m pip install --quiet -r "$REQUIREMENTS" 2>/dev/null; then
        if ! "$PYTHON" -m pip install --quiet --break-system-packages \
                -r "$REQUIREMENTS" 2>/dev/null; then
            log "WARNING: Could not install Python dependencies automatically."
            log "         Run manually: pip install -r $REQUIREMENTS"
            log "         Or create a virtualenv and activate it before running this script."
        fi
    fi
fi

# ---- Locate test suites -----------------------------------------------------
# The suite directory is discovered rather than hard-coded: an explicit -s (or
# XPU_SMI_SUITES) wins, otherwise pick the single immediate subdirectory that
# holds YAML suites. Internal-only suite directories are stripped from the
# public tree, so naming one here would break this script after that filtering.
if [[ -z "$SUITES_DIR" ]]; then
    mapfile -t CANDIDATES < <(
        find "$SCRIPT_DIR" -mindepth 2 -maxdepth 2 -name '*.yaml' -printf '%h\n' \
            | sort -u
    )
    case "${#CANDIDATES[@]}" in
        0) die "No YAML suite directory found under $SCRIPT_DIR.
Specify one with -s <dir> or set XPU_SMI_SUITES." ;;
        1) SUITES_DIR="${CANDIDATES[0]}" ;;
        *) die "Multiple YAML suite directories found under $SCRIPT_DIR:
$(printf '  %s\n' "${CANDIDATES[@]}")
Pick one with -s <dir> or set XPU_SMI_SUITES." ;;
    esac
fi

E2E_TESTS_DIR="$(realpath -m "$SUITES_DIR" 2>/dev/null || echo "$SUITES_DIR")"
[[ -d "$E2E_TESTS_DIR" ]] || die "E2E test directory not found: $E2E_TESTS_DIR"

YAML_COUNT="$(find "$E2E_TESTS_DIR" -maxdepth 1 -name '*.yaml' | wc -l)"
[[ "$YAML_COUNT" -gt 0 ]] || die "No YAML test suites in $E2E_TESTS_DIR"
log "Found $YAML_COUNT YAML test suites in $E2E_TESTS_DIR"

# ---- Build run_tests.py arguments -------------------------------------------
RUN_TESTS="${SCRIPT_DIR}/run_tests.py"
[[ -f "$RUN_TESTS" ]] || die "run_tests.py not found at $RUN_TESTS"

ARGS=(
    --config  "$E2E_TESTS_DIR"
    --binary  "$BINARY"
    --issues-dir "$ISSUES_DIR"
    --log-file   "$LOG_FILE"
    --junit-xml  "$JUNIT_XML"
    --summary-json "$SUMMARY_JSON"
    --parallel   "$PARALLEL"
)

if [[ -n "$TAGS" ]]; then
    # TAGS is comma-separated; split into individual --tags arguments
    IFS=',' read -ra TAG_ARRAY <<< "$TAGS"
    ARGS+=(--tags "${TAG_ARRAY[@]}")
fi

[[ "$VERBOSE" == true ]] && ARGS+=(--verbose)

# ---- Run --------------------------------------------------------------------
log "Starting e2e run at $(date)"
echo ""
set +e
"$PYTHON" "$RUN_TESTS" "${ARGS[@]}"
EXIT_CODE=$?
set -e

echo ""
log "Run completed at $(date)"

# ---- Report issue files -----------------------------------------------------
ISSUE_COUNT=0
if [[ -d "$ISSUES_DIR" ]]; then
    ISSUE_COUNT="$(find "$ISSUES_DIR" -maxdepth 1 -name '*.txt' | wc -l)"
fi

if [[ "$ISSUE_COUNT" -gt 0 ]]; then
    log "Issues found: $ISSUE_COUNT"
    log "Issue files:"
    find "$ISSUES_DIR" -maxdepth 1 -name '*.txt' | sort | while read -r f; do
        echo "  $f"
    done
else
    log "No issue files written (all tests passed or no failures produced output)."
fi

log "Full log: $LOG_FILE"
exit "$EXIT_CODE"
