# xpu-smi CLI Validation Framework

A YAML-driven test framework for end-to-end validation of the `xpu-smi`
command-line tool.  Tests cover every subcommand with valid and invalid inputs,
verify exit codes, check for human-readable output, and flag internal debug
messages that leak into user-facing output.

---

## Directory layout

```
validation/
├── run_tests.py            # Core test runner (YAML → execute → validate)
├── run_e2e.sh              # One-shot e2e orchestration script
├── run_coverage.py         # Build + unit-test + e2e + gcovr orchestrator
├── coverage_filters.py     # Shared gcovr include/exclude patterns
├── requirements.txt        # Python dependencies
├── example_test_schema.yaml  # Annotated reference for writing tests
├── lib/
│   ├── runner.py           # CLITestRunner: loads YAML, runs tests, collects results
│   ├── validators.py       # Return-code, plaintext, JSON, CSV, combined-output validators
│   ├── executor.py         # Subprocess wrapper (captures stdout+stderr, enforces timeout)
│   ├── models.py           # TestResult dataclass
│   ├── resolver.py         # Dependency resolution / parameterised-test ordering
│   ├── color.py            # ANSI colour helpers (auto-disabled when not a TTY)
│   └── utils.py            # YAML loading, device discovery helpers
└── embargo/                # End-to-end test suites (one per subcommand + global)
    ├── global.yaml
    ├── discovery.yaml
    ├── topology.yaml
    ├── health.yaml
    ├── stats.yaml
    ├── ps.yaml
    ├── dump.yaml
    ├── config.yaml
    ├── vgpu.yaml
    ├── log.yaml
    ├── updatefw.yaml
    ├── amc.yaml
    ├── listpciinfo.yaml
    └── ...                  # plus coverage/parity suites (json_schema, *_extended, etc.)
```

---

## Quick start

### 1. Install `uv` (if not already present)

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

### 2. Create a virtual environment and install dependencies

```bash
cd validation/
uv venv .venv
uv pip install -r requirements.txt
source .venv/bin/activate
```

### 3. Build the binary

```bash
cd ../builddir/
meson compile
```

### 4. Run the full e2e suite

```bash
cd ../validation/
bash run_e2e.sh
```

Issue report files are written to `e2e_results_<timestamp>/issues/`.

---

## Running with `run_e2e.sh`

`run_e2e.sh` is the recommended entry point.  It locates all YAML suites in
the suite directory, runs them through `run_tests.py`, and writes per-failure report
files.

```
Usage: run_e2e.sh [options]

  -b <path>   Path to xpu-smi binary
              Default: ../builddir/ial/cli/xpu-smi (relative to this script)
              Override with: XPU_SMI=/path/to/xpu-smi ./run_e2e.sh
  -o <dir>    Output directory for logs and issue files
              Default: e2e_results_<timestamp>/
  -j <N>      Run up to N tests in parallel  (default: 1)
  -t <tags>   Comma-separated tags to run, e.g. sanity,error
  -v          Verbose: print truncated stdout/stderr for each test
  -h          Show help
```

Examples:

```bash
# Default run against the standard build path
bash run_e2e.sh

# Custom binary, parallel execution, custom output directory
bash run_e2e.sh -b /opt/xpu-smi/xpu-smi -j 4 -o /tmp/myrun

# Run only tests tagged 'sanity'
bash run_e2e.sh -t sanity

# Use the XPU_SMI environment variable
XPU_SMI=/usr/local/bin/xpu-smi bash run_e2e.sh
```

Exit codes: `0` = all passed, `1` = failures found, `2` = setup error.

---

## Running with `run_tests.py` directly

`run_tests.py` gives finer control: select individual suites, filter by tag,
adjust parallelism, or write a log file.

```bash
# Activate virtualenv first
source .venv/bin/activate

# Single suite
python run_tests.py -c embargo/discovery.yaml \
                    -b ../builddir/ial/cli/xpu-smi

# All e2e suites, parallel, with issue files
python run_tests.py -c embargo/ -r \
                    -b ../builddir/ial/cli/xpu-smi \
                    -j 4 \
                    --issues-dir /tmp/issues \
                    --log-file /tmp/run.log

# Only sanity-tagged tests across all suites
python run_tests.py -c embargo/ -r \
                    -b ../builddir/ial/cli/xpu-smi \
                    --tags sanity

# Reproduce one failing test by name, with verbose logs
python run_tests.py -c embargo/dump_metrics_matrix.yaml \
                    -b ../builddir/ial/cli/xpu-smi \
                    -k dump_date_prefix -v

# CI run: skip tests that modify system configuration
python run_tests.py -c embargo/ -r \
                    -b ../builddir/ial/cli/xpu-smi \
                    --exclude-tags intrusive

# List all tests without running them
python run_tests.py -c embargo/ -r --list-tests
```

### Key flags

| Flag | Description |
|------|-------------|
| `-c / --config` | YAML file, directory, glob, or comma-separated list |
| `-r / --recursive` | Recurse into subdirectories when `--config` is a directory |
| `-b / --binary` | Path to the `xpu-smi` binary under test (default: `xpu-smi` from `PATH`) |
| `-p / --platform` | Platform matched against the `platform:` filter in test YAML (default: `BMG`) |
| `-t / --tags` | Run only tests whose `tags:` list matches any given value |
| `--exclude-tags` | Skip tests carrying any given tag (CI: `--exclude-tags intrusive`) |
| `-k / --test-name` | Run only tests whose name contains a given substring — reproduce a single failing test, e.g. `-k dump_date_prefix -v` |
| `-j / --parallel` | Thread-pool size for concurrent test execution |
| `--issues-dir` | Write a `.txt` report file per failure into this directory |
| `--junit-xml` | Write a JUnit XML report for CI dashboards |
| `--summary-json` | Write a machine-readable JSON summary of every result |
| `--log-file` | Plain-text timestamped log (in addition to console output) |
| `-v / --verbose` | Show truncated stdout/stderr for every test as it runs |
| `--list-tests` | Print test names and descriptions, then exit |

`--tags` and `-k/--test-name` are applied *after* parameterised expansion, so an
individual instance such as `dump_modes[mode=0]` can be selected by name.  Both
filters run *before* dependency resolution, so a filter set must also include
any `depends_on` prerequisites of the tests it selects.

---

## Issue report files

When `--issues-dir` is specified (or when using `run_e2e.sh`), every failed
test produces a plain-text report in `<issues-dir>/<test_name>.txt`:

```
Test:        discovery_device_out_of_range
Command:     /path/to/xpu-smi discovery -d 999

Expected:
  return_code: [1, 2, 255]
  ...

Actual:
  Exit code: 0
  Stdout: ...
  Stderr: ...

Explanation:
  Return code mismatch: expected [1, 2, 255], got 0
```

Each file contains everything needed to reproduce the issue: the exact command,
what was expected, what was observed, and why it fails.

---

## Writing tests

All tests live in YAML files with this top-level shape:

```yaml
test_suite:
  name: "Suite name"

  settings:
    timeout: 30               # per-test timeout in seconds
    continue_on_failure: true

    # Suite-wide bad-output guards (inherited by every test):
    default_combined_not_contains:
      - "Error: %s"           # unformatted printf string in output
    default_combined_not_matches_regex:
      - '\[(?:Error|Warning|Info)\] \.\.\/'   # source-file paths
      - 'ZE_RESULT_[A-Z0-9_]+'               # Level Zero internal constants

  tests:
    - name: "my_test"
      command: "discovery -d 0"
      expect:
        return_code: 0
        stdout_contains:
          - "Device Name"
      tags: ["sanity"]
```

### Validation keys under `expect:`

| Key | Description |
|-----|-------------|
| `return_code` | Integer or list of accepted integers |
| `stdout_contains` | List of strings, all must appear in stdout |
| `stdout_not_contains` | List of strings, none may appear in stdout |
| `stdout_matches_regex` | List of regexes, every one must match stdout (re.search, MULTILINE) |
| `stdout_not_matches_regex` | List of regexes, none may match stdout |
| `stdout_equals` | Exact stdout match (trailing newlines ignored) |
| `stdout_or_stderr_contains` | List of strings, all must appear in stdout OR stderr |
| `stderr_contains` | List of strings, all must appear in stderr |
| `stderr_not_contains` | List of strings, none may appear in stderr |
| `stderr_matches_regex` | List of regexes, every one must match stderr |
| `stderr_not_matches_regex` | List of regexes, none may match stderr |
| `stderr_equals` | Exact stderr match (trailing newlines ignored) |
| `stderr_empty` | Boolean — when `true`, stderr must be empty after `.strip()` |
| `combined_not_contains` | List of literals banned from stdout+stderr combined |
| `combined_not_matches_regex` | List of regexes banned from stdout+stderr combined |
| `json_schema` | JSON Schema object validated against stdout (with `output_format: json`) |
| `json_path_checks` | List of `{ path, expected, value? }` JSONPath checks |
| `csv_required_columns` | List of header names that must be present (with `output_format: csv`) |
| `csv_min_rows` | Minimum data-row count |
| `csv_row_count` | Exact data-row count |
| `csv_column_not_empty` | List of columns whose every cell must be non-empty |

Set `output_format` (test-level, outside `expect:`) to one of:

- `plaintext` (default) — stdout treated as text; the `stdout_*` keys apply
- `json` — stdout parsed as JSON before `json_schema` / `json_path_checks` run; a
  parse failure is itself a test failure. Catches cases where `[Error]` log
  lines or CSV rows appear where JSON is expected.
- `csv` — stdout parsed as CSV; the `csv_*` keys apply

JSONPath `expected` operators: `exists`, `equals` (with `value:`), `all_equal` (with `value:`).

### Suite-level default bad-output guards

Every suite in the suite directory sets `default_combined_not_matches_regex` and
`default_combined_not_contains` in its `settings:` block.  These patterns are
automatically merged into the `expect:` of each test in the suite, so a single
bad-output occurrence fails the test even if `stdout_contains` would otherwise
pass.

The three patterns applied globally across all e2e suites:

| Pattern | What it catches |
|---------|----------------|
| `\[(?:Error\|Warning\|Info)\] \.\.\/` | Internal log lines with source-file paths, e.g. `[Error] ../hal/core/process.cpp:27:` |
| `ZE_RESULT_[A-Z0-9_]+` | Level Zero internal error constants, e.g. `ZE_RESULT_ERROR_UNSUPPORTED_FEATURE` |
| `XPUM_RESULT_[A-Z0-9_]+` | XPUM internal result constants, e.g. `XPUM_RESULT_OK` |
| `"Error: %s"` (literal) | Unformatted printf format string leaked into output |

### Capability gating — `requires_capability`

Some tests need hardware or privileges that not every DUT has.  Rather than
letting them fail on a machine that legitimately cannot run them, gate them with
`requires_capability` (a single string or a list).  A test whose capability is
unavailable is reported as **skipped**, not failed:

```yaml
    - name: "discovery_device_1"
      description: "discovery --device 1 must exit cleanly (requires a second GPU)"
      command: "discovery --device 1"
      requires_capability: multi_device
      expect:
        return_code: 0
      tags: ["e2e"]
```

| Capability | Available when | Probe |
|------------|----------------|-------|
| `multi_device` | Two or more GPUs are discoverable | `discovery -j` returns ≥ 2 `device_list` entries |
| `amc` | The platform reports a non-empty AMC firmware version | `discovery --listamcversions -j` returns a non-empty version list |
| `sriov` | Running as root on a host with an SR-IOV-capable PF | any `/sys/class/drm/card*/device/sriov_totalvfs` reads > 0 |

Each probe runs at most once per session and the result is cached, so gating a
hundred tests on `multi_device` costs one `discovery` call.  An unrecognised
capability name logs a warning and is treated as unavailable — the test is
skipped rather than silently passing.

### OS and platform gates

`os:` and `platform:` skip a test on a non-matching host (each accepts a string
or a list).  Set them per test, or once for a whole suite under
`test_suite.settings` — the suite value is applied as a default to every test
that does not specify its own:

```yaml
test_suite:
  name: "E2E: cri_po"
  settings:
    os: "Linux"          # must be under settings:, not directly on test_suite:
    platform: ["BMG"]
```

`platform:` is matched against `-p/--platform` (default `BMG`); `os:` is matched
against the detected host OS.  Both gates are honoured by command tests and
workflow tests alike.

### Shell scripts as test bodies

`script:` runs an arbitrary shell script instead of invoking the binary
directly — useful for loops over discovered BDFs, redirection, or backgrounding
a monitor process.  The binary is **not** prepended; write `{command}` where the
`xpu-smi <command>` string should be substituted, and preserve the exit code:

```yaml
    - name: "cri_po_discovery_bdf"
      command: "discovery"
      script: |
        set -u
        bdfs=$(lspci -nn | grep -Ei '8086:(E20B|56C1)' | cut -d' ' -f1)
        if [ -z "$bdfs" ]; then
          echo "No Intel GPU BDF found; skipping BDF-specific command"
          exit 0
        fi
        for bdf in $bdfs; do
          {command} --device "$bdf" || exit $?
        done
      expect:
        return_code: 0
```

`script:` is also valid inside a workflow `steps:` entry, which is how cleanup
steps remove scratch files — pair it with `cleanup_step: true` and
`run_on_failure: true` so the scratch file is removed even when the step under
test failed:

```yaml
        - name: "cleanup_dump_file"
          cleanup_step: true
          run_on_failure: true
          command: ""
          script: "rm -f /tmp/e2e_dump_test.csv"
          expect:
            return_code: [0, 1]
```

### Other test features

See `example_test_schema.yaml` for full annotated examples of:

- **Parameterised tests** — explicit list, numeric range, or auto-discovered device IDs
- **Parallel matrix** — run all parameter variants concurrently with `parallel: true`
- **Workflow tests** — multi-step sequences with background steps, `depends_on`,
  `cleanup_step`, `run_on_failure`, `condition`, and `startup_return_code` for
  background steps
- **Wrapper** — wrap a command with another tool (e.g. `perf stat`, `timeout`)
- **Hooks** — structured `before_test` / `after_test` / `on_failure` shell snippets
- **Variables** — OS-specific value substitution
- **Retries** — `max_retries` (per test or in `settings:`) re-runs a flaky test
  before declaring failure
- **Suite-level setup / teardown** — `test_suite.setup` and `test_suite.teardown`
  fields execute a shell command once before / after the suite runs (e.g. to
  bring up a daemon or clean up scratch directories). A non-zero exit from
  setup skips the suite; teardown always runs.

### Reports for CI

`run_tests.py` produces three machine-readable artefacts:

| Flag | Output |
|------|--------|
| `--issues-dir DIR`    | Per-failure plain-text reports with command, expected vs actual |
| `--junit-xml PATH`    | JUnit XML — drop-in for any CI dashboard |
| `--summary-json PATH` | Single JSON file with totals + per-test rc/duration/message |

`run_e2e.sh` always writes all three under its output directory.

---

## Coverage runs with `run_coverage.py`

`run_coverage.py` is the OS-agnostic build-test-measure orchestrator.  In one
invocation it configures a coverage build, clears stale `.gcda`, runs the
doctest C++ unit tests, runs the YAML e2e suites against the freshly built
binary, and produces a `gcovr` report over the merged coverage from both phases:

```bash
python3 run_coverage.py -c embargo/ -j 4
```

| Flag | Description |
|------|-------------|
| `-b / --build-dir` | Meson build directory (default: `builddir`) |
| `-c / --config` | YAML suite directory or single file (default: auto-detect `tests/e2e`, then `tests`) |
| `-o / --output-dir` | Report output directory (default: `validation_results_<timestamp>`) |
| `-j / --parallel` | Parallel e2e workers |
| `-t / --tags` | Comma-separated tag filter passed through to `run_tests.py` |
| `-s / --skip-build` | Skip the rebuild (the binary must already be a coverage build) |
| `--skip-unit-tests` | Skip the doctest phase |
| `--skip-e2e-tests` | Skip the YAML e2e phase |
| `--skip-tests` | Report only, assuming `.gcda` files are already present |

The suite directory is **not** auto-detected for this repository layout — pass
`-c embargo/` explicitly.  Exit codes match `run_e2e.sh`: `0` pass, `1` test
failures, `2` setup/build/coverage error.

The report is filtered to *relevant* code through `coverage_filters.py`, which
excludes the tests themselves, deprecated subcommands slated for removal, the C
API shim, and firmware paths that need out-of-band hardware.  Both this script
and any CI integration import that one list, so console and dashboard numbers
always agree.

> `gcovr` resolves paths relative to `--root`, so keep exactly one meson build
> directory in the repo root while measuring; two build dirs make it attribute
> the same source file twice and the run fails.

---

## Tagging convention

Tags fall into two groups.  **Selector tags** describe *what kind* of test it is
and are the ones worth filtering on:

| Tag | Meaning |
|-----|---------|
| `e2e` | All tests in the e2e suite (applied to every test) |
| `sanity` | Quick smoke tests (help, version, basic invocation) |
| `error` | Invalid-input and error-handling tests |
| `negative` | Malformed-argument hardening (duplicate/negative ids, overflow, empty BDF) |
| `json` | Tests that request JSON output and validate its schema |
| `csv` | Tests for CSV output format (header row content, multi-column headers) |
| `schema` | Deep JSON-schema validation of a command's full output shape |
| `stderr` | Stream-discipline tests (what belongs on stdout vs stderr) |
| `coverage` | Read-only variants added purely to broaden relevant-code coverage |
| `regression` | Guards against a specific previously-fixed defect |
| `workflow` | Multi-step workflow tests rather than single commands |
| `concurrency` | Multiple `xpu-smi` processes running against one device |
| `stress` | Long-running or high-sample-count invocations |
| `intrusive` | Tests that attempt to change device configuration (may need permissions) |
| `sudo` | Requires root; skipped or failed on an unprivileged host |
| `hardware` | Depends on a specific hardware feature being present |

**Subject tags** name the subcommand or feature under test and exist so a single
area can be re-run in isolation: `amc`, `api`, `bdf`, `component`, `config`,
`count`, `cri_po`, `device`, `device-id`, `discovery`, `dump`, `env`, `health`,
`help`, `listpciinfo`, `log`, `metric`, `ps`, `query-gpu`, `stats`, `topology`,
`version`.

Run a subset:
```bash
bash run_e2e.sh -t sanity
bash run_e2e.sh -t error
# multiple tags: any match runs the test
python run_tests.py -c embargo/ -r --tags sanity json
# everything except tests that mutate device state or need root
python run_tests.py -c embargo/ -r --exclude-tags intrusive sudo
```

---

## CI integration

A typical CI job using `uv`:

```yaml
# .github/workflows/e2e.yml (example)
steps:
  - uses: actions/checkout@v4
  - name: Install uv
    run: curl -LsSf https://astral.sh/uv/install.sh | sh
  - name: Set up Python env
    run: |
      cd validation
      uv venv .venv
      uv pip install -r requirements.txt
  - name: Build
    run: cd builddir && meson compile
  - name: Run e2e tests
    run: |
      cd validation
      source .venv/bin/activate
      bash run_e2e.sh -b ../builddir/ial/cli/xpu-smi \
                      -o /tmp/e2e_results \
                      -j $(nproc)
  - name: Upload issue reports
    if: failure()
    uses: actions/upload-artifact@v4
    with:
      name: xpu-smi-issues
      path: /tmp/e2e_results/issues/
```

---

## Adding tests for a new subcommand

1. Create `embargo/<subcommand>.yaml` using the template below.
2. The `settings:` block with bad-output guards is required — copy it verbatim.
3. Tag all tests with `e2e` plus appropriate secondary tags.

```yaml
test_suite:
  name: "E2E: <subcommand>"
  description: "Tests for xpu-smi <subcommand>"

  settings:
    timeout: 30
    continue_on_failure: true
    default_combined_not_contains:
      - "Error: %s"
    default_combined_not_matches_regex:
      - '\[(?:Error|Warning|Info)\] \.\.\/'
      - 'ZE_RESULT_[A-Z0-9_]+'
      - 'XPUM_RESULT_[A-Z0-9_]+'

  tests:
    - name: "<subcommand>_help"
      description: "Help text must be shown and exit 0"
      command: "<subcommand> --help"
      expect:
        return_code: 0
        stdout_contains:
          - "Usage"
      tags: ["e2e", "sanity"]

    - name: "<subcommand>_invalid_flag"
      description: "Unknown flag must exit non-zero"
      command: "<subcommand> --invalid-flag-xyz"
      expect:
        return_code: [1, 2, 255]
      tags: ["e2e", "error"]
```
