#!/usr/bin/env python3
# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

"""
Main entry point for running CLI validation tests against the xpu-smi binary.

Test suites are defined in YAML files. Each suite contains tests of two types:
  - command: single command run against the binary, with output validation
  - workflow: multi-step sequence with shared context, background steps, and
              conditional/cleanup/recovery step logic

Tests support: parameterization (explicit list, range, auto-discover),
parallel execution, dependency ordering (depends_on), OS/platform filters,
and output validation for plaintext, JSON schema, and JSONPath checks.

See tests/example_test_schema.yaml for a full reference of all options.

Usage:
    python run_tests.py --config tests/discovery/discovery_tests.yaml
    python run_tests.py --config tests/ --tags sanity
    python run_tests.py --config tests/ --binary /path/to/xpu-smi --verbose
    python run_tests.py --config tests/ -j 4
    python run_tests.py --config file1.yaml,file2.yaml
"""

import argparse
import sys
import glob
from pathlib import Path

# Add the validation/ directory so lib is importable as a package
sys.path.insert(0, str(Path(__file__).parent))

from lib.runner import CLITestRunner


def expand_config_paths(config_args, recursive=False):
    """
    Expand config arguments into list of file paths.
    Supports:
    - Multiple --config arguments
    - Comma-separated values: file1.yaml,file2.yaml
    - Glob patterns: config/*.yaml
    - Directories: config/ (finds all .yaml files in directory)
    - Recursive directories: --recursive flag
    
    Args:
        config_args: List of config argument strings
        recursive: If True, search directories recursively
    
    Returns:
        List of resolved file paths
    """
    expanded_paths = []
    
    for arg in config_args:
        # Split comma-separated values
        for path_pattern in arg.split(','):
            path_pattern = path_pattern.strip()
            if not path_pattern:
                continue
            
            path_obj = Path(path_pattern)
            
            # Check if it's a directory
            if path_obj.is_dir():
                # Find all YAML files in directory
                if recursive:
                    # Recursive search
                    yaml_files = list(path_obj.rglob('*.yaml')) + list(path_obj.rglob('*.yml'))
                else:
                    # Non-recursive (immediate children only)
                    yaml_files = list(path_obj.glob('*.yaml')) + list(path_obj.glob('*.yml'))
                
                # Convert to strings and sort
                expanded_paths.extend(sorted(str(f) for f in yaml_files))
            else:
                # Try glob pattern expansion
                matches = glob.glob(path_pattern)
                if matches:
                    # Glob found matches
                    expanded_paths.extend(sorted(matches))
                else:
                    # No glob match - treat as literal path (may not exist yet)
                    expanded_paths.append(path_pattern)
    
    return expanded_paths


def main():
    parser = argparse.ArgumentParser(
        description='Run CLI validation tests from YAML configuration',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run all tests from a single config file
  python run_tests.py --config tests/discovery/discovery_tests.yaml

  # Run all tests in a directory (non-recursive)
  python run_tests.py --config tests/

  # Run all tests in a directory tree recursively
  python run_tests.py --config tests/ --recursive
  python run_tests.py --config tests/ -r

  # Multiple configs: repeated flags, comma-separated, or glob
  python run_tests.py --config tests/discovery/ --config tests/diag/
  python run_tests.py --config tests/discovery/discovery_tests.yaml,tests/diag/test_diag.yaml
  python run_tests.py --config "tests/*_tests.yaml"

  # Filter by tag  (any test whose tags list contains any of these values)
  python run_tests.py --config tests/ --tags sanity
  python run_tests.py --config tests/ --tags sanity json workflow

  # Override the binary under test
  python run_tests.py --config tests/ --binary /opt/xpu-smi/xpu-smi

  # Specify target platform (controls platform: filter in test YAML)
  python run_tests.py --config tests/ --platform BMG

  # Suite-level parallelism: run up to N tests concurrently
  python run_tests.py --config tests/ --parallel 4
  python run_tests.py --config tests/ -j $(nproc)

  # List tests without running them (shows pre-expansion names)
  python run_tests.py --config tests/ --list-tests

  # Verbose: log debug output and truncated stdout/stderr on failure
  python run_tests.py --config tests/ --verbose

  # Write all output to a log file (plain text, timestamped)
  python run_tests.py --config tests/ --log-file results/run.log
  python run_tests.py --config tests/ -j 4 --log-file results/ci_run.log

YAML test types (see tests/example_test_schema.yaml for full reference):
  type: command   Single xpu-smi invocation with output validation (default)
  type: workflow  Multi-step sequence with shared context; supports background
                  steps, conditional/cleanup/recovery logic, and dependency
                  ordering via depends_on:

Output validation options (under expect:):
  return_code, stdout_contains, stdout_not_contains, stdout_or_stderr_contains,
  json_schema, json_path_checks (exists / equals / all_equal)

Parameterization (command tests):
  parameters + command_template expand to one test per value combination.
  Parameter types: explicit list, range (start/end/step), auto_discover_devices.
  Add parallel: true to run all variants of a single test concurrently.
        """
    )
    
    parser.add_argument(
        '-c', '--config',
        action='append',
        required=True,
        help='Path(s) to YAML test configuration file(s). Accepts: a single file, a directory '
             '(all .yaml/.yml files in it), a glob pattern (e.g. "tests/*_tests.yaml"), '
             'comma-separated paths, or multiple --config flags. Use -r to search directories '
             'recursively.'
    )
    
    parser.add_argument(
        '-r', '--recursive',
        action='store_true',
        help='When --config is a directory, search recursively for YAML files'
    )
    
    parser.add_argument(
        '-b', '--binary',
        default='xpu-smi',
        help='Path to the xpu-smi binary (default: xpu-smi, resolved from PATH). '
             'Supports ~ and environment variables. This binary is prepended to every '
             'command: field in the YAML; use script: to run arbitrary shell commands instead.'
    )
    
    parser.add_argument(
        '-p', '--platform',
        default='BMG',
        choices=['BMG'],
        help='Platform type to match against the platform: filter in test YAML (default: BMG). '
             'Tests whose platform: list does not include this value are silently skipped.'
    )
    
    parser.add_argument(
        '-t', '--tags',
        nargs='+',
        help='Run only tests whose tags: list contains at least one of the given values. '
             'Applies to both command and workflow tests. Parameterized tests are expanded '
             'before filtering, so tag filtering matches the expanded instances.'
    )
    
    parser.add_argument(
        '-j', '--parallel',
        type=int,
        default=1,
        metavar='N',
        help='Run up to N tests concurrently using a thread pool (default: 1 = sequential). '
             'Independent of per-test parallelism: tests with parallel: true always run '
             'their parameter variants concurrently regardless of this flag.'
    )
    
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Enable verbose output'
    )
    
    parser.add_argument(
        '--list-tests',
        action='store_true',
        help='Print all test names, descriptions, and tags without running them. '
             'Note: shows the raw (pre-expansion) test definitions; parameterized tests '
             'appear once even though they expand to multiple instances at runtime.'
    )

    parser.add_argument(
        '--log-file',
        default=None,
        metavar='PATH',
        help='Write all output to this file in addition to the console. '
             'The file uses plain text with timestamps (no ANSI color codes). '
             'Parent directories are created automatically if they do not exist.'
    )
    
    parser.add_argument(
        '--issues-dir',
        default=None,
        metavar='PATH',
        help='Write a per-issue report file for every failed test into this directory. '
             'Each file is named after the test and contains the command, expected result, '
             'actual output, and explanation. The directory is created if it does not exist.'
    )

    parser.add_argument(
        '--junit-xml',
        default=None,
        metavar='PATH',
        help='Write a JUnit-style XML report for CI consumption to this path. '
             'Each test becomes a <testcase>; failures embed stdout/stderr.'
    )

    parser.add_argument(
        '--summary-json',
        default=None,
        metavar='PATH',
        help='Write a machine-readable JSON summary of all results to this path.'
    )
    
    args = parser.parse_args()
    
    # Expand config paths (handles multiple args, comma-separated, globs, and directories)
    config_paths = expand_config_paths(args.config, recursive=args.recursive)
    
    if not config_paths:
        print("Error: No config files found", file=sys.stderr)
        return 1
    
    # Check for duplicates
    unique_paths = []
    seen = set()
    for path in config_paths:
        normalized = str(Path(path).resolve())
        if normalized not in seen:
            seen.add(normalized)
            unique_paths.append(path)
    
    config_paths = unique_paths
    
    if len(config_paths) > 1:
        print(f"Running tests from {len(config_paths)} configuration files:")
        for path in config_paths:
            print(f"  - {path}")
        print()
    
    # Expand user path (~ and environment variables) in binary path
    import os
    binary_path = os.path.expanduser(os.path.expandvars(args.binary))

    # Create log file parent directory if needed
    if args.log_file:
        Path(args.log_file).parent.mkdir(parents=True, exist_ok=True)

    # Initialize test runner
    runner = CLITestRunner(binary_path=binary_path, verbose=args.verbose,
                          platform_type=args.platform, max_workers=args.parallel,
                          log_file=args.log_file,
                          issues_dir=args.issues_dir)
    
    # Show parallel mode if enabled
    if args.parallel > 1:
        print(f"Running tests in parallel mode with {args.parallel} workers\n")
    
    # List tests if requested
    if args.list_tests:
        total_tests = 0
        for config_path in config_paths:
            try:
                config = runner.load_test_config(config_path)
                tests = config.get('test_suite', {}).get('tests', [])
                suite_name = config.get('test_suite', {}).get('name', 'Unknown Suite')
                
                if len(config_paths) > 1:
                    print(f"\n{config_path} ({suite_name}):")
                    print(f"  Found {len(tests)} tests:")
                else:
                    print(f"Found {len(tests)} tests:")
                
                for i, test in enumerate(tests, 1):
                    name = test.get('name', 'unnamed')
                    desc = test.get('description', '')
                    tags = test.get('tags', [])
                    prefix = "    " if len(config_paths) > 1 else "  "
                    print(f"{prefix}{i}. {name}")
                    if desc:
                        print(f"{prefix}   {desc}")
                    if tags:
                        print(f"{prefix}   Tags: {', '.join(tags)}")
                
                total_tests += len(tests)
            except Exception as e:
                print(f"Error loading {config_path}: {e}", file=sys.stderr)
        
        if len(config_paths) > 1:
            print(f"\nTotal tests across all configs: {total_tests}")
        return 0
    
    # Run tests from all config files
    all_results = []
    failed_configs = []
    
    for config_path in config_paths:
        try:
            if len(config_paths) > 1:
                print(f"\n{'='*70}")
                print(f"Loading configuration: {config_path}")
                print('='*70)
            
            config = runner.load_test_config(config_path)
            results = runner.run_test_suite(config, filter_tags=args.tags)
            all_results.extend(results)
            
        except Exception as e:
            print(f"Error loading/running {config_path}: {e}", file=sys.stderr)
            if args.verbose:
                import traceback
                traceback.print_exc()
            failed_configs.append(config_path)
    
    # Print combined summary
    if len(config_paths) > 1:
        print(f"\n\n{'='*70}")
        print("COMBINED SUMMARY - ALL CONFIGURATIONS")
        print('='*70)
    
    runner.print_summary(all_results)
    
    if failed_configs:
        print(f"\nFailed to load/run {len(failed_configs)} configuration(s):")
        for path in failed_configs:
            print(f"  - {path}")

    if args.issues_dir and any(not r.passed for r in all_results):
        print(f"\nIssue files written to: {args.issues_dir}")

    if args.junit_xml:
        runner.write_junit_xml(all_results, args.junit_xml)

    if args.summary_json:
        runner.write_summary_json(all_results, args.summary_json)
    
    # Return non-zero exit code if any test failed or configs couldn't be loaded
    if any(not r.passed for r in all_results) or failed_configs:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
