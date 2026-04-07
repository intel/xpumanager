# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

"""
Output validators for CLI test results.
Handles plaintext string matching and JSON schema / JSONPath validation.
"""

import json
import logging
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
    """Validate plaintext output against stdout_contains / stdout_not_contains."""
    failures = []

    for pattern in expectations.get('stdout_contains', []):
        if pattern not in output:
            failures.append(f"Expected pattern not found: '{pattern}'")

    for pattern in expectations.get('stdout_not_contains', []):
        if pattern in output:
            failures.append(f"Unexpected pattern found: '{pattern}'")

    if failures:
        return False, "; ".join(failures)
    return True, "All plaintext validations passed"


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
