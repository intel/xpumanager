#!/usr/bin/python3
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
"""
Helper script to output XPUMD config file with its "intel_xpu_status" section
rules & conditions overwritten by ones taken from "xpu-smi" metric thresholds JSON file.

Usage:
	convert-thresholds.py <config.yaml> <thresholds.json>

Example:
	scripts/convert-thresholds.py config-example.yaml ../resources/config/device_thresholds.json > new.yaml
"""

import json
import os
import sys
from typing import Never, TextIO

def error_exit(msg: str) -> Never:
    "show usage, error message and exit with 1"
    sys.stderr.write(__doc__)
    sys.stderr.write(f"\nERROR: {msg}!\n")
    sys.exit(1)


# --- current config handling ---

SECTION_ID = "intel_xpu_status:"

def conf_has_section(path: str) -> bool:
    "check that given config includes required section heading"
    for line in open(path, "r", encoding="utf-8"):
        if line.strip() == SECTION_ID:
            return True
    return False

# output conf until section, skip the current section and return next section start
def output_until_section(f: TextIO) -> (int, str):
    "output given file content until required section starts, and skip to section end"
    indent = 0
    found = False
    for line in f:
        stripped = line.strip()
        # skip rest of the section?
        if found:
            if not stripped:
                continue
            # lazy way to check whether this is higher indentation
            if line.index(stripped) > indent:
                continue
            # next section
            return indent, line
        # output everything until the section + section heading
        print(line, end="")
        if stripped == SECTION_ID:
            indent = line.index(SECTION_ID)
            print(f"{' '*(indent+2)}rules:")
            found = True
    if found:
        return indent, ""
    error_exit("config file is missing required section '{SECTION_ID}'")

# output rest of conf after the section replacement is output
def output_after_section(f: TextIO, headline: str) -> None:
    "output head line following required section, and rest of the config as-is"
    print("\n" + headline, end="")
    for line in f:
        print(line, end="")


# --- check arg files & load JSON file ---

if len(sys.argv) != 3:
    error_exit("incorrect number of args")

conf_path = sys.argv[1]
json_path = sys.argv[2]
if not os.path.exists(conf_path):
    error_exit(f"specified conf file does not exist: {conf_path}")
if not os.path.exists(json_path):
    error_exit(f"specified thresholds file does not exist: {json_path}")

if not conf_has_section(conf_path):
    error_exit(f"specified conf file misses require {SECTION_ID} {conf_path}")

def check_duplicate_keys(pairs):
    # JSON loading silently overrides dict content with later key values
    "hook function for JSON loading that exits with error if it duplicates keys"
    result_dict = {}
    for key, val in pairs:
        if key in result_dict:
            error_exit(f"duplicate JSON key: {key}")
        result_dict[key] = val
    return result_dict

try:
    loaded = json.load(open(json_path, "r", encoding="utf-8"), object_pairs_hook=check_duplicate_keys)
except json.decoder.JSONDecodeError as err:
    error_exit(f"JSON decoding failed for '{json_path}': {err}")


# --- check / get relevant thresholds from JSON file ---

DEFAULTS = "default_thresholds"

defaults = {}
if DEFAULTS in loaded:
    defaults = loaded[DEFAULTS]
    del loaded[DEFAULTS]
else:
    error_exit(f"thresholds file is missing '{DEFAULTS}' values")

thresholds = {}
for metric_name, limits in loaded.items():
    # check intermediate metric level name, and pick limits under it as their names are unique
    if not metric_name.endswith("_thresholds"):
        error_exit(f"metric threshold limits has no '_thresholds' suffix for '{metric_name}'")
    for limit_name, items in limits.items():
        if not items:
            error_exit(f"JSON '{limit_name}' dict lists no limits")
        thresholds[limit_name] = items


# --- mappings for known xpu-smi thresholds ---

# mappings for different warning levels of the same metric need to:
# - be in increasing severity order
# - have identical "name" value
# - either "sensor" or "aggfunc" specified
RULE_MAP = [
    {
        "name":   "core temperature", # rule name
        "thkey":  "throttle_core",    # key in (xpu-smi config) thresholds dicts
        "defkey": "core_throttle",    # key in (xpu-smi config) defaults dict
        "metric": "hw.temperature",   # OTel src metric
        "sensor": "gpu",              # its sensor location
        "hwtype": "temperature",      # hw.type for produced hw.status OTel metric
        "level":  "warning",          # status severity level
    },
    {
        "name":   "core temperature",
        "thkey":  "shutdown_core",
        "defkey": "core_shutdown",
        "metric": "hw.temperature",
        "sensor": "gpu",
        "hwtype": "temperature",
        "level":  "critical",
    },
    {
        "name":   "memory temperature",
        "thkey":  "shutdown_memory",
        "defkey": "memory_shutdown",
        "metric": "hw.temperature",
        "sensor": "memory",
        "hwtype": "temperature",
        "level":  "critical",
    },
    {
        "name":   "power usage",
        "thkey":  "tdps",
        "defkey": "power_tdp",
        "metric": "hw.power",
        # use following function to aggregate values over all sensors
        "aggfunc": "max",
        "hwtype": "gpu",
        "level":  "warning",
    }
]

def is_same_metric(prev: dict, current: dict) -> bool:
    "return true if metric infos match between given previous and current rules + do few checks"
    if not prev:
        return False
    same_name = prev["name"] == current["name"]
    for key in ("metric", "sensor", "hwtype"):
        if prev[key] != current[key]:
            if same_name:
                error_exit(f"internal rule mismatch, same name but different metric:\n{prev}\n{current}")
            return False
    if not same_name:
        error_exit(f"internal rule mismatch, same metric but different name:\n{prev}\n{current}")
    return True

# --- output templates for those thresholds ---

# indent from section header
RULE_INDENT = 4
RULE_TEMPLATE = """\
- name: "Intel GPU {name} health {level} threshold(s)"
  source_metric: "{metric}"
  parent_metric: "hw.gpu.info"
  parent_ref_attribute: "hw.id"
  parent_filters:
    - key: "pci.vendor_id"
      values: ["8086"]{filters}
  copy_attributes: ["hw.id", "hw.name", "pci.bdf", "hw.sensor_location", "com.intel.subdevice_id"]
  add_attributes:
    hw.type: "{hwtype}"
  # Ordered list of state rules. The rules should be ordered by increasing severity.
  # All rules are evaluated, the last matching one will be active.
  states:
    # Default state
    - state_name: "ok"\
"""

AGGREGATE_TEMPLATE = """
  aggregate:
    function: "{aggfunc}"
    group_by: ["hw.sensor_location"]\
"""

FILTERS_TEMPLATE = """
  component_filters:
    - key: "hw.sensor_location"
      values: ["{sensor}"]\
"""

STATE_INDENT = 8
STATE_TEMPLATE = """\
- state_name: "%s"
  conditions:\
"""

COND_INDENT = 12
COND_TEMPLATE = """\
- value: {value}
  parent_filters:
    - key: "pci.device_id"
      values: [{devids}]\
"""

COND_DEFAULT_TEMPLATE = """\
# Default {name} {level} threshold for other device IDs
- value: {value}\
"""

def print_indented(count: int, template: str) -> None:
    "print lines in given (template) string with specified indentation"
    spaces = ' '*count
    #sys.stderr.write(f"{spaces}{count}\n")
    for line in template.splitlines():
        print(f"{spaces}{line}")

# --- output conf with replacement status rules, their states and conditions ---

old_conf = open(conf_path, "r", encoding="utf-8")
section_indent, nextline = output_until_section(old_conf)

prev_rule = {}
for rule in RULE_MAP:
    thkey = rule["thkey"]
    if not thkey in thresholds:
        error_exit(f"thresholds file missing '{thkey}'")
    values = {}
    # collect quoted + prefix-removed device IDs, per threshold value
    for devid, value in thresholds[thkey].items():
        if value not in values:
            values[value] = {}
        if devid.startswith("0x"):
            devid = devid[2:]
        values[value][f'"{devid.lower()}"'] = True
    if len(values) == 0:
        sys.stderr.write(f"skipping '{thkey}', no threshold values\n")
        continue
    if not is_same_metric(prev_rule, rule):
        if "aggfunc" in rule and rule["aggfunc"]:
            rule["aggfunc"] = rule["aggfunc"].capitalize()
            rule["filters"] = AGGREGATE_TEMPLATE.format_map(rule)
        else:
            rule["filters"] = FILTERS_TEMPLATE.format_map(rule)
        print_indented(section_indent+RULE_INDENT, RULE_TEMPLATE.format_map(rule))
        prev_rule = rule
    print_indented(section_indent+STATE_INDENT, STATE_TEMPLATE % rule["level"])
    # JSON items are key'd by device ID so there cannot be overlapping ones,
    # but order conditions from largest to smallest value for clarity
    for value, devids in reversed(sorted(values.items())):
        condvars = {"value": value, "devids": ', '.join(devids.keys())}
        print_indented(section_indent+COND_INDENT, COND_TEMPLATE.format_map(condvars))
    defkey = rule["defkey"]
    if defkey in defaults:
        values = {"name": rule["name"], "level": rule["level"], "value": defaults[defkey]}
        print_indented(section_indent+COND_INDENT, COND_DEFAULT_TEMPLATE.format_map(values))

output_after_section(old_conf, nextline)
