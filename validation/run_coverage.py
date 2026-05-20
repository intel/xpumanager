#!/usr/bin/env python3
# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT
"""run_coverage.py — Build, test and coverage orchestrator for xpu-smi.

OS-agnostic replacement for run_validation.sh.  Performs:

  1. ``meson setup -Db_coverage=true`` (if needed) + ``ninja``
  2. Delete stale ``*.gcda`` files
  3. Run ``meson test`` (doctest C++ unit tests) — populates .gcda
  4. Invoke ``run_tests.py`` against the YAML e2e test suites
  5. Print pass/fail summary
  6. Run ``gcovr`` with the curated relevant-coverage filters
     (see ``coverage_filters.py``) over the merged .gcda from both
     phases to produce console / HTML / JSON / Cobertura XML reports

Works on any platform where meson, ninja and gcovr are available.

Usage:
    python3 run_coverage.py [options]

Options:
    -b, --build-dir DIR    Meson build directory (default: builddir)
    -c, --config PATH      YAML test directory or single config file
                           (default: auto-detect tests/e2e or tests)
    -o, --output-dir DIR   Output dir for reports
                           (default: validation_results_<timestamp>)
    -j, --parallel N       Parallel test workers (default: 1)
    -t, --tags TAGS        Comma-separated tag filter
    -s, --skip-build       Skip rebuild (binary must already be a coverage build)
        --skip-tests       Run coverage report only (assumes .gcda already present)
        --skip-unit-tests  Skip the doctest C++ unit-test phase
        --skip-e2e-tests   Skip the YAML e2e validation phase
    -v, --verbose          Verbose test output

Exit codes:
    0  All tests passed
    1  One or more tests failed
    2  Setup / build / coverage error
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

from coverage_filters import gcovr_filter_args

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------
def log(msg: str) -> None:
    print(f"[run_coverage] {datetime.now().strftime('%H:%M:%S')} {msg}", flush=True)


def die(msg: str, code: int = 2) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(code)


def require(cmd: str) -> str:
    """Return absolute path to ``cmd`` or exit 2."""
    found = shutil.which(cmd)
    if not found:
        die(f"{cmd} not found in PATH")
    return found


def run(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    """Echo + run; raise on non-zero unless ``check=False`` passed."""
    log("$ " + " ".join(str(c) for c in cmd))
    return subprocess.run(cmd, **kw)


# ---------------------------------------------------------------------------
# argv
# ---------------------------------------------------------------------------
def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Run xpu-smi YAML tests + relevant-code coverage report",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("-b", "--build-dir", default="builddir",
                   help="Meson build directory (default: builddir)")
    p.add_argument("-c", "--config",
                   help="YAML test directory or single config file")
    p.add_argument("-o", "--output-dir",
                   help="Output directory for reports")
    p.add_argument("-j", "--parallel", type=int, default=1,
                   help="Parallel test workers (default: 1)")
    p.add_argument("-t", "--tags",
                   help="Comma-separated tag filter")
    p.add_argument("-s", "--skip-build", action="store_true",
                   help="Skip rebuild (binary must already be coverage-instrumented)")
    p.add_argument("--skip-tests", action="store_true",
                   help="Skip tests, only generate coverage report from existing .gcda")
    p.add_argument("--skip-unit-tests", action="store_true",
                   help="Skip the doctest C++ unit-test phase")
    p.add_argument("--skip-e2e-tests", action="store_true",
                   help="Skip the YAML e2e validation phase")
    p.add_argument("-v", "--verbose", action="store_true",
                   help="Verbose test output")
    return p.parse_args()


# ---------------------------------------------------------------------------
# phases
# ---------------------------------------------------------------------------
def phase_build(build_dir: Path) -> None:
    log("=== Phase 1: Building xpu-smi with coverage instrumentation ===")
    require("meson")
    require("ninja")

    if not (build_dir / "meson-info").is_dir():
        log("Build directory not configured. Running meson setup...")
        run(["meson", "setup", str(build_dir), "-Db_coverage=true"],
            cwd=REPO_ROOT, check=True)
    else:
        cfg = subprocess.run(["meson", "configure", str(build_dir)],
                             capture_output=True, text=True)
        opts = {}
        for line in cfg.stdout.splitlines():
            parts = line.split()
            if len(parts) >= 2 and parts[0] in ("b_coverage", "with_tests"):
                opts[parts[0]] = parts[1]
        if opts.get("b_coverage") != "true":
            log("Enabling -Db_coverage=true ...")
            run(["meson", "configure", str(build_dir), "-Db_coverage=true"],
                check=True)
        if opts.get("with_tests") != "true":
            log("Note: unit tests are not enabled (`with_tests=false`). "
                "To include doctest coverage, reconfigure with:")
            log("      conan install . --output-folder=.conan/ --build=missing "
                "-c tools.build:skip_test=False")
            log(f"      meson configure {build_dir} -Dwith_tests=true")

    log("Compiling...")
    if run(["ninja", "-C", str(build_dir)]).returncode != 0:
        die("Build failed — see output above")
    log("Build succeeded")


def phase_clear_gcda(build_dir: Path) -> int:
    log("=== Phase 2: Clearing previous coverage data ===")
    removed = 0
    for path in build_dir.rglob("*.gcda"):
        try:
            path.unlink()
            removed += 1
        except OSError:
            pass
    gcno = sum(1 for _ in build_dir.rglob("*.gcno"))
    log(f"Removed {removed} stale .gcda; {gcno} .gcno instrumentation files present")
    return gcno


def resolve_test_dir(override: str | None) -> Path:
    if override:
        path = Path(override)
        if not path.exists():
            die(f"Config path not found: {override}")
        return path
    for cand in (SCRIPT_DIR / "tests" / "e2e", SCRIPT_DIR / "tests"):
        if cand.is_dir():
            return cand
    die("Test directory not found. Use -c to specify an explicit path.")


def phase_unit_tests(build_dir: Path, paths: dict[str, Path],
                     verbose: bool) -> int:
    """Run meson test (doctest C++ unit tests). .gcda lands in the same
    build dir as the e2e run, so gcovr later sees the merged coverage."""
    log("=== Phase 3a: Running C++ unit tests (doctest via meson test) ===")
    require("meson")

    # Detect whether unit tests are actually registered (with_tests=true).
    listing = subprocess.run(["meson", "test", "-C", str(build_dir), "--list"],
                             capture_output=True, text=True)
    registered = [ln for ln in listing.stdout.splitlines()
                  if ln.strip() and not ln.startswith("No tests")]
    if not registered:
        log("No unit tests registered in this build dir. "
            "Reconfigure with `-Dwith_tests=true` (or rerun without `-s`) "
            "to include doctest coverage.")
        return 0
    log(f"Unit-test targets registered: {len(registered)}")

    cmd = ["meson", "test", "-C", str(build_dir), "--print-errorlogs"]
    if verbose:
        cmd.append("--verbose")

    unit_log = paths["output"] / "unit_tests.log"
    with unit_log.open("wb") as fh:
        cp = subprocess.run(cmd, stdout=fh, stderr=subprocess.STDOUT)

    # Parse meson's stdout for a quick summary (Ok:/Fail: line).
    summary_line = ""
    try:
        for line in unit_log.read_text().splitlines():
            if line.strip().startswith(("Ok:", "Fail:", "Expected Fail:")) or \
               "tests passed" in line.lower() or "summary" in line.lower():
                summary_line += line + "\n"
    except OSError:
        pass

    # Copy meson's machine-readable test log if produced.
    meson_log = build_dir / "meson-logs" / "testlog.txt"
    if meson_log.is_file():
        try:
            shutil.copy2(meson_log, paths["output"] / "unit_tests_meson.txt")
        except OSError:
            pass

    if cp.returncode == 0:
        log("Unit tests: PASSED")
    else:
        log(f"Unit tests: FAILED (exit {cp.returncode}) — see {unit_log}")
    if summary_line:
        sys.stdout.write(summary_line)
    return cp.returncode


def phase_run_tests(args, binary: Path, paths: dict[str, Path]) -> int:
    log("=== Phase 3b: Running YAML e2e validation tests ===")
    test_dir = resolve_test_dir(args.config)
    run_tests = SCRIPT_DIR / "run_tests.py"
    if not run_tests.is_file():
        die(f"run_tests.py not found: {run_tests}")

    if test_dir.is_dir():
        yaml_count = sum(1 for _ in test_dir.glob("*.yaml"))
    else:
        yaml_count = 1
    log(f"Test suites found: {yaml_count}")
    if yaml_count == 0:
        die(f"No YAML test suites found in {test_dir}.")

    cmd = [
        sys.executable, str(run_tests),
        "--config", str(test_dir),
        "--binary", str(binary),
        "--issues-dir", str(paths["issues"]),
        "--log-file", str(paths["log"]),
        "--junit-xml", str(paths["junit"]),
        "--summary-json", str(paths["summary"]),
        "--parallel", str(args.parallel),
    ]
    if args.tags:
        cmd += ["--tags", *args.tags.split(",")]
    if args.verbose:
        cmd.append("--verbose")

    return subprocess.run(cmd).returncode


def phase_test_summary(summary_json: Path) -> None:
    log("=== Phase 4: Test Results ===")
    if not summary_json.is_file():
        log(f"WARNING: summary JSON not found: {summary_json}")
        return
    with summary_json.open() as f:
        s = json.load(f)
    total = s["total"]
    passed = s["passed"]
    failed = s["failed"]
    dur = s.get("duration", 0.0)
    rate = (passed / total * 100) if total else 0.0

    print("=" * 70)
    print("  TEST RESULTS SUMMARY")
    print("=" * 70)
    print(f"  Total tests : {total}")
    print(f"  Passed      : {passed:>4d}  ({rate:5.1f}%)")
    print(f"  Failed      : {failed:>4d}  ({100 - rate:5.1f}%)")
    print(f"  Duration    : {dur:.1f}s")
    print("-" * 70)
    if failed:
        print("\n  FAILED TESTS:")
        for t in s["tests"]:
            if not t["passed"]:
                rc = t.get("return_code", "?")
                msg = (t.get("message") or "")[:120]
                print(f"    [FAIL] {t['name']}")
                print(f"           rc={rc}  {msg}")
        print()
    print("=" * 70)


def phase_coverage(build_dir: Path, paths: dict[str, Path]) -> None:
    log("=== Phase 5: Code Coverage Report ===")
    gcda_count = sum(1 for _ in build_dir.rglob("*.gcda"))
    log(f"Coverage data files (.gcda): {gcda_count}")
    if gcda_count == 0:
        log("WARNING: No coverage data found. "
            "Was the binary built with -Db_coverage=true and exercised?")
        log("         Skipping coverage report.")
        return

    require("gcovr")
    require("gcov")

    common = [
        "gcovr",
        "--root", str(REPO_ROOT),
        "--gcov-executable", "gcov",
        *gcovr_filter_args(),
        "--gcov-ignore-parse-errors=negative_hits.warn_once_per_file",
        "--gcov-ignore-errors=no_working_dir_found",
        str(build_dir),
    ]

    # 1. Console summary (tee to file)
    log("Generating console summary...")
    cp = subprocess.run(common + ["--sort-percentage", "--print-summary"],
                        capture_output=True, text=True)
    paths["cov_summary"].write_text(cp.stdout)
    sys.stdout.write(cp.stdout)
    if cp.returncode != 0:
        sys.stderr.write(cp.stderr)
        die("gcovr (console summary) failed")

    err_log = paths["output"] / "gcovr.err"

    def gcovr_extra(label: str, extra: list[str]) -> None:
        log(f"Generating {label} coverage report...")
        with err_log.open("ab") as elog:
            rc = subprocess.run(common + extra, stderr=elog).returncode
        if rc != 0:
            log(f"ERROR: gcovr ({label}) failed; see {err_log}")
            try:
                tail = err_log.read_text().splitlines()[-20:]
                sys.stderr.write("\n".join(tail) + "\n")
            except OSError:
                pass
            die(f"gcovr ({label}) failed")

    gcovr_extra("HTML", ["--html", "--html-details",
                         "--output", str(paths["cov_html"] / "index.html")])
    log(f"HTML report: {paths['cov_html'] / 'index.html'}")

    gcovr_extra("JSON", ["--json", "--output", str(paths["cov_json"])])
    log(f"JSON report: {paths['cov_json']}")

    cov_xml = paths["output"] / "coverage.xml"
    gcovr_extra("XML", ["--xml", "--output", str(cov_xml)])
    log(f"XML report: {cov_xml}")

    # Per-file table
    print()
    print("=" * 74)
    print("  CODE COVERAGE — PER-FILE BREAKDOWN (sorted by coverage %)")
    print("=" * 74)
    cp = subprocess.run(common + ["--sort-percentage"],
                        capture_output=True, text=True)
    sys.stdout.write("\n".join(cp.stdout.splitlines()[:120]) + "\n")
    print("=" * 74)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main() -> int:
    args = parse_args()

    build_dir = (REPO_ROOT / args.build_dir).resolve()
    binary = build_dir / "ial" / "cli" / "xpu-smi"
    if os.name == "nt":
        binary = binary.with_suffix(".exe")

    timestamp = time.strftime("%Y%m%d_%H%M%S")
    output_dir = Path(args.output_dir).resolve() if args.output_dir else \
        (REPO_ROOT / f"validation_results_{timestamp}").resolve()

    paths = {
        "output":      output_dir,
        "issues":      output_dir / "issues",
        "log":         output_dir / "test_run.log",
        "junit":       output_dir / "junit.xml",
        "summary":     output_dir / "summary.json",
        "cov_html":    output_dir / "coverage_html",
        "cov_json":    output_dir / "coverage.json",
        "cov_summary": output_dir / "coverage_summary.txt",
    }
    for d in (paths["output"], paths["issues"], paths["cov_html"]):
        d.mkdir(parents=True, exist_ok=True)

    log(f"Repository root  : {REPO_ROOT}")
    log(f"Build directory  : {build_dir}")
    log(f"Output directory : {output_dir}")
    print()

    # Build
    if args.skip_build or args.skip_tests:
        log("=== Phase 1: SKIPPED (using existing binary) ===")
    else:
        phase_build(build_dir)

    if not binary.is_file() or not os.access(binary, os.X_OK):
        die(f"xpu-smi binary not found or not executable: {binary}")
    log(f"Binary: {binary}")
    print()

    # Clear stale coverage
    if not args.skip_tests:
        phase_clear_gcda(build_dir)
        print()

    # Tests
    test_exit = 0
    unit_exit = 0
    if not args.skip_tests:
        if not args.skip_unit_tests:
            unit_exit = phase_unit_tests(build_dir, paths, args.verbose)
            print()
        if not args.skip_e2e_tests:
            test_exit = phase_run_tests(args, binary, paths)
            print()
            phase_test_summary(paths["summary"])
            log(f"Full log     : {paths['log']}")
            log(f"JUnit XML    : {paths['junit']}")
            log(f"Summary JSON : {paths['summary']}")
            if paths["issues"].is_dir():
                n = sum(1 for _ in paths["issues"].glob("*.txt"))
                log(f"Issue files  : {n} (in {paths['issues']}/)")
            print()

    # Coverage
    phase_coverage(build_dir, paths)
    print()

    # Final summary
    log("=== Final Summary ===")
    print()
    print(f"  Reports written to: {output_dir}/")
    print()
    if not args.skip_unit_tests and not args.skip_tests:
        print("  Unit test reports:")
        print(f"    - meson testlog   : {paths['output'] / 'unit_tests.log'}")
        print()
    if not args.skip_e2e_tests and not args.skip_tests:
        print("  E2E test reports:")
        print(f"    - Console log     : {paths['log']}")
        print(f"    - JUnit XML       : {paths['junit']}")
        print(f"    - Summary JSON    : {paths['summary']}")
        print(f"    - Issue files     : {paths['issues']}/")
        print()
    print("  Coverage reports:")
    print(f"    - Console summary : {paths['cov_summary']}")
    print(f"    - HTML report     : {paths['cov_html'] / 'index.html'}")
    print(f"    - JSON report     : {paths['cov_json']}")
    print()
    overall = test_exit or unit_exit
    if overall == 0:
        log("RESULT: ALL TESTS PASSED (unit + e2e)")
    else:
        log(f"RESULT: SOME TESTS FAILED (unit={unit_exit} e2e={test_exit})")
    return overall


if __name__ == "__main__":
    sys.exit(main())
