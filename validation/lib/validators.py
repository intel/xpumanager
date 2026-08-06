# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

"""
Output validators for CLI test results.
Handles plaintext string matching and JSON schema / JSONPath validation.
"""

import json
import logging
import re
from typing import Any, Dict, List, Tuple, Union

import jsonschema
from jsonpath_ng.ext import parse

logger = logging.getLogger("CLITestRunner")


def validate_return_code(actual: int, expected: Union[int, List[int]]) -> bool:
    """Return True if actual matches the expected return code(s)."""
    if isinstance(expected, list):
        return actual in expected
    return actual == expected


def validate_plaintext(output: str, expectations: Dict[str, Any]) -> Tuple[bool, str]:
    """Validate plaintext stdout against literal and regex expectations.

    Supported keys:
      stdout_contains         list[str] — every literal must be present in stdout
      stdout_not_contains     list[str] — every literal must NOT be present
      stdout_matches_regex    list[str] — every regex must match (re.search, MULTILINE)
      stdout_not_matches_regex list[str] — no regex may match
      stdout_equals           str       — exact match (after rstrip of trailing newline)
    """
    failures = []

    for pattern in expectations.get('stdout_contains', []):
        if pattern not in output:
            failures.append(f"Expected pattern not found: '{pattern}'")

    for pattern in expectations.get('stdout_not_contains', []):
        if pattern in output:
            failures.append(f"Unexpected pattern found: '{pattern}'")

    for regex_str in expectations.get('stdout_matches_regex', []):
        try:
            if not re.search(regex_str, output, re.MULTILINE):
                failures.append(f"Expected regex did not match stdout: {regex_str!r}")
        except re.error as exc:
            failures.append(f"Invalid regex {regex_str!r}: {exc}")

    for regex_str in expectations.get('stdout_not_matches_regex', []):
        try:
            m = re.search(regex_str, output, re.MULTILINE)
            if m:
                failures.append(
                    f"Forbidden regex {regex_str!r} matched stdout: {m.group(0)!r}"
                )
        except re.error as exc:
            failures.append(f"Invalid regex {regex_str!r}: {exc}")

    if 'stdout_equals' in expectations:
        expected = expectations['stdout_equals']
        if output.rstrip('\r\n') != expected.rstrip('\r\n'):
            failures.append(
                f"stdout exact-match failed:\n  expected: {expected!r}\n  actual:   {output!r}"
            )

    if failures:
        return False, "; ".join(failures)
    return True, "All plaintext validations passed"


def validate_stderr(stderr: str, expectations: Dict[str, Any]) -> Tuple[bool, str]:
    """Validate stderr against literal/regex/exact-match expectations.

    Supported keys:
      stderr_contains         list[str]
      stderr_not_contains     list[str]
      stderr_matches_regex    list[str]
      stderr_not_matches_regex list[str]
      stderr_equals           str (rstrip-newline match)
      stderr_empty            bool — when True, stderr must be empty (after strip)
    """
    failures = []

    for pattern in expectations.get('stderr_contains', []):
        if pattern not in stderr:
            failures.append(f"Expected stderr pattern not found: '{pattern}'")

    for pattern in expectations.get('stderr_not_contains', []):
        if pattern in stderr:
            failures.append(f"Unexpected stderr pattern found: '{pattern}'")

    for regex_str in expectations.get('stderr_matches_regex', []):
        try:
            if not re.search(regex_str, stderr, re.MULTILINE):
                failures.append(f"Expected regex did not match stderr: {regex_str!r}")
        except re.error as exc:
            failures.append(f"Invalid regex {regex_str!r}: {exc}")

    for regex_str in expectations.get('stderr_not_matches_regex', []):
        try:
            m = re.search(regex_str, stderr, re.MULTILINE)
            if m:
                failures.append(
                    f"Forbidden regex {regex_str!r} matched stderr: {m.group(0)!r}"
                )
        except re.error as exc:
            failures.append(f"Invalid regex {regex_str!r}: {exc}")

    if 'stderr_equals' in expectations:
        expected = expectations['stderr_equals']
        if stderr.rstrip('\r\n') != expected.rstrip('\r\n'):
            failures.append(
                f"stderr exact-match failed:\n  expected: {expected!r}\n  actual:   {stderr!r}"
            )

    if expectations.get('stderr_empty') is True:
        if stderr.strip():
            failures.append(f"stderr expected to be empty but had: {stderr.strip()[:200]!r}")

    if failures:
        return False, "; ".join(failures)
    return True, "All stderr validations passed"


def validate_csv_output(output: str, expectations: Dict[str, Any]) -> Tuple[bool, str]:
    """Validate CSV output.

    Supported keys:
      csv_min_rows            int  — at least this many data rows (excluding header)
      csv_required_columns    list[str] — header must contain each of these columns
      csv_row_count           int  — exact data-row count
      csv_column_not_empty    list[str] — every value in given columns must be non-empty
    """
    import csv as _csv
    from io import StringIO

    # Drop empty leading lines before the CSV header row
    lines = output.splitlines()
    while lines and not lines[0].strip():
        lines.pop(0)

    if not lines:
        return False, "CSV output is empty"

    try:
        reader = _csv.DictReader(StringIO('\n'.join(lines)))
        rows = list(reader)
        header = reader.fieldnames or []
    except Exception as e:
        return False, f"CSV parse failed: {e}"

    failures = []

    required = expectations.get('csv_required_columns', [])
    for col in required:
        if col not in header:
            failures.append(f"Missing CSV column: {col!r} (header: {header})")

    if 'csv_min_rows' in expectations:
        if len(rows) < expectations['csv_min_rows']:
            failures.append(f"Expected at least {expectations['csv_min_rows']} rows, got {len(rows)}")

    if 'csv_row_count' in expectations:
        if len(rows) != expectations['csv_row_count']:
            failures.append(f"Expected exactly {expectations['csv_row_count']} rows, got {len(rows)}")

    for col in expectations.get('csv_column_not_empty', []):
        if col not in header:
            failures.append(f"csv_column_not_empty references unknown column: {col!r}")
            continue
        empties = [i for i, r in enumerate(rows) if not str(r.get(col, '')).strip()]
        if empties:
            failures.append(
                f"CSV column {col!r} has empty values in rows {empties[:5]}"
                f"{' (and more)' if len(empties) > 5 else ''}"
            )

    if failures:
        return False, "; ".join(failures)
    return True, f"CSV validations passed ({len(rows)} rows, {len(header)} columns)"


def validate_combined_output(stdout: str, stderr: str,
                              expectations: Dict[str, Any]) -> Tuple[bool, str]:
    """
    Validate that forbidden patterns appear in NEITHER stdout NOR stderr.

    Checks:
      combined_not_contains:       list of literal strings that must NOT appear
      combined_not_matches_regex:  list of regex patterns that must NOT match

    Each stream is examined independently rather than concatenated.  This
    avoids two failure modes of the naive "stdout + stderr" approach:
      (a) a forbidden literal/regex that straddles the stream boundary
          (last partial line of stdout + first line of stderr) producing a
          false positive that does not correspond to any real line, and
      (b) the inserted separator suppressing a legitimate match when stdout
          ends without a trailing newline.

    Designed to catch internal debug output that leaks into user-facing output:
      - Source file paths:   [Error] ../path/file.cpp:line:
      - L0 constants:        ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
      - Unformatted strings: Error: %s
    """
    failures = []
    streams = (('stdout', stdout), ('stderr', stderr))

    for pattern in expectations.get('combined_not_contains', []):
        # Aggregate stream names so a literal that appears in both stdout and
        # stderr produces a single failure entry rather than two duplicates.
        hits = []
        sample_line = pattern
        for stream_name, text in streams:
            if pattern in text:
                hits.append(stream_name)
                if sample_line is pattern:
                    sample_line = next(
                        (ln for ln in text.splitlines() if pattern in ln),
                        pattern,
                    )
        if hits:
            where = " and ".join(hits)
            failures.append(
                f"Forbidden literal found in {where}: {pattern!r}  "
                f"(line: {sample_line.strip()!r})"
            )

    for regex_str in expectations.get('combined_not_matches_regex', []):
        try:
            compiled = re.compile(regex_str, re.MULTILINE)
        except re.error as exc:
            failures.append(f"Invalid regex {regex_str!r}: {exc}")
            continue
        hits = []
        offending = None
        sample_line = None
        for stream_name, text in streams:
            m = compiled.search(text)
            if m:
                hits.append(stream_name)
                if offending is None:
                    offending = m.group(0)
                    sample_line = next(
                        (ln for ln in text.splitlines() if offending in ln),
                        offending,
                    )
        if hits:
            where = " and ".join(hits)
            failures.append(
                f"Forbidden regex {regex_str!r} matched in {where}: "
                f"{offending!r}  (line: {sample_line.strip()!r})"
            )

    if failures:
        return False, "; ".join(failures)
    return True, "Combined output validation passed"


def _parse_json(output: str) -> Tuple[Any, str]:
    """
    Parse JSON from command output.
    Handles mixed output by scanning for the first '{' or '[' line.

    Returns:
        (parsed_data, error_message) — error_message is empty on success.
    """
    try:
        return json.loads(output), ""
    except json.JSONDecodeError as first_err:
        lines = output.split('\n')
        for i, line in enumerate(lines):
            stripped = line.strip()
            if stripped and stripped[0] in ('{', '['):
                try:
                    return json.loads('\n'.join(lines[i:])), ""
                except json.JSONDecodeError:
                    break
        return None, f"Invalid JSON output: {first_err}"


def validate_json_output(output: str, expectations: Dict[str, Any]) -> Tuple[bool, str]:
    """Validate JSON output against json_schema and/or json_path_checks."""
    json_data, err = _parse_json(output)
    if err:
        logger.debug(f"JSON parse failed. First 200 chars: {output[:200]}")
        return False, err

    failures = []

    if 'json_schema' in expectations:
        try:
            jsonschema.validate(instance=json_data, schema=expectations['json_schema'])
        except jsonschema.ValidationError as e:
            failures.append(f"JSON schema validation failed: {e.message}")

    for check in expectations.get('json_path_checks', []):
        path = check.get('path')
        expected_type = check.get('expected')
        expected_value = check.get('value')
        description = check.get('description', path)

        try:
            matches = parse(path).find(json_data)

            if expected_type == 'exists':
                if not matches:
                    failures.append(f"{description}: Path not found")

            elif expected_type == 'equals':
                if not matches:
                    failures.append(f"{description}: Path not found")
                elif matches[0].value != expected_value:
                    failures.append(
                        f"{description}: Expected {expected_value!r}, got {matches[0].value!r}"
                    )

            elif expected_type == 'all_equal':
                if not matches:
                    failures.append(f"{description}: Path not found")
                else:
                    values = [m.value for m in matches]
                    if not all(v == expected_value for v in values):
                        bad = [v for v in values if v != expected_value]
                        failures.append(
                            f"{description}: Not all values equal {expected_value!r} "
                            f"(mismatches: {bad[:5]})"
                        )

        except Exception as e:
            failures.append(f"{description}: JSONPath evaluation error: {e}")

    if failures:
        return False, "; ".join(failures)
    return True, "All JSON validations passed"
