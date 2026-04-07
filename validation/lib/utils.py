# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

"""
Utility functions for CLI validation testing.
"""

import json
import yaml
import re
from typing import Any, Dict, List, Optional


def parse_bdf_address(bdf: str) -> Optional[Dict[str, str]]:
    """
    Parse BDF (Bus:Device.Function) address.
    
    Args:
        bdf: BDF string like "0000:03:00.0"
    
    Returns:
        Dict with domain, bus, device, function or None if invalid
    """
    pattern = r'([0-9a-fA-F]{4}):([0-9a-fA-F]{2}):([0-9a-fA-F]{2})\.([0-9a-fA-F])'
    match = re.match(pattern, bdf)
    if match:
        return {
            'domain': match.group(1),
            'bus': match.group(2),
            'device': match.group(3),
            'function': match.group(4)
        }
    return None


def extract_device_ids(json_output: str) -> List[int]:
    """
    Extract device IDs from JSON output.
    
    Args:
        json_output: JSON string from xpu-smi command
    
    Returns:
        List of device IDs
    """
    try:
        data = json.loads(json_output)
        if 'device_list' in data:
            return [d.get('device_id') for d in data['device_list'] if 'device_id' in d]
        elif 'device_id' in data:
            return [data['device_id']]
    except (json.JSONDecodeError, KeyError):
        pass
    return []


def format_output_table(data: List[Dict[str, Any]], columns: List[str]) -> str:
    """
    Format data as a text table.
    
    Args:
        data: List of dictionaries
        columns: Column names to display
    
    Returns:
        Formatted table string
    """
    if not data:
        return "No data"
    
    # Calculate column widths
    widths = {col: len(col) for col in columns}
    for row in data:
        for col in columns:
            value = str(row.get(col, ''))
            widths[col] = max(widths[col], len(value))
    
    # Build header
    header = ' | '.join(col.ljust(widths[col]) for col in columns)
    separator = '-+-'.join('-' * widths[col] for col in columns)
    
    # Build rows
    rows = []
    for row in data:
        rows.append(' | '.join(str(row.get(col, '')).ljust(widths[col]) for col in columns))
    
    return '\n'.join([header, separator] + rows)


def compare_versions(version1: str, version2: str) -> int:
    """
    Compare two version strings.
    
    Args:
        version1: First version string (e.g., "1.2.3")
        version2: Second version string (e.g., "1.2.4")
    
    Returns:
        -1 if version1 < version2, 0 if equal, 1 if version1 > version2
    """
    def normalize(v):
        return [int(x) for x in re.sub(r'[^0-9.]', '', v).split('.') if x]
    
    parts1 = normalize(version1)
    parts2 = normalize(version2)
    
    for i in range(max(len(parts1), len(parts2))):
        v1 = parts1[i] if i < len(parts1) else 0
        v2 = parts2[i] if i < len(parts2) else 0
        if v1 < v2:
            return -1
        elif v1 > v2:
            return 1
    return 0


def sanitize_json_string(text: str) -> str:
    """
    Clean up text that might contain invalid JSON formatting.
    Useful for extracting JSON from mixed output.
    
    Args:
        text: Input text potentially containing JSON
    
    Returns:
        Cleaned JSON string
    """
    # Find JSON object or array
    json_match = re.search(r'(\{.*\}|\[.*\])', text, re.DOTALL)
    if json_match:
        return json_match.group(1)
    return text


def validate_pci_device_id(device_id: str) -> bool:
    """
    Validate PCI device ID format.
    
    Args:
        device_id: Device ID string like "0x56c0"
    
    Returns:
        True if valid format
    """
    pattern = r'^0x[0-9a-fA-F]{4}$'
    return bool(re.match(pattern, device_id))


class OutputFormatter:
    """Helper class for formatting test output."""
    
    @staticmethod
    def format_pass(message: str) -> str:
        """Format a pass message with color (if terminal supports it)."""
        return f"{message}"
    
    @staticmethod
    def format_fail(message: str) -> str:
        """Format a fail message with color (if terminal supports it)."""
        return f"{message}"
    
    @staticmethod
    def format_skip(message: str) -> str:
        """Format a skip message."""
        return f"{message}"
    
    @staticmethod
    def format_section(title: str) -> str:
        """Format a section header."""
        return f"\n{'=' * 60}\n{title}\n{'=' * 60}"


def load_yaml_safe(file_path: str) -> Dict[str, Any]:
    """
    Safely load YAML file.
    
    Args:
        file_path: Path to YAML file
    
    Returns:
        Parsed YAML data
    
    Raises:
        Exception if loading fails
    """
    with open(file_path, 'r', encoding='utf-8') as f:
        return yaml.safe_load(f)


def save_test_results(results: List[Any], output_path: str, format: str = 'json'):
    """
    Save test results to file.
    
    Args:
        results: List of test results
        output_path: Output file path
        format: Output format ('json' or 'yaml')
    """
    data = [
        {
            'test_name': r.test_name,
            'passed': r.passed,
            'message': r.message,
            'duration': r.duration
        }
        for r in results
    ]
    
    with open(output_path, 'w', encoding='utf-8') as f:
        if format == 'yaml':
            yaml.dump(data, f, default_flow_style=False)
        else:
            json.dump(data, f, indent=2)
