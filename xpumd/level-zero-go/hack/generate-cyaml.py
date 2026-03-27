#!/usr/bin/env python3
"""
Generate CYAML schemas for structs.

Annotations supported in header comments (// gen: <directive>):
  flatten    - flatten this inline struct member's sub-members with dot-path prefix
  count=NAME - for fixed arrays: use field NAME as the runtime element count
  ignore     - skip this field entirely (no CYAML schema entry generated)
  key=NAME   - on a struct field: use NAME as the YAML key instead of the auto-generated CamelCase name
               on an enum value: use NAME as the YAML string for that value instead of the C enumerator name
  enum       - emit a CYAML_VALUE_ENUM string table for this enum type

Usage:
    python3 hack/generate-cyaml.py [options]
"""

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

@dataclass
class Annotations:
    """Parsed // gen: directives for a struct field."""
    ignore: bool = False
    flatten: bool = False
    count: str = ""
    key: str = ""


# Hardcoded annotations for upstream headers we cannot modify
# TODO: consider moving these into a separate config file
FIELD_ANNOTATIONS_OVERRIDE = {
    ("zes_fan_speed_table_t", "table"): Annotations(count="numPoints"),
    # UUID fields in upstream structs are handled via sysman_uuid_t wrapper fields;
    # skip them here to avoid generating a CYAML schema for the raw byte-array struct.
    ("ze_device_properties_t", "uuid"): Annotations(ignore=True),
    ("zes_device_ext_properties_t", "uuid"): Annotations(ignore=True),
    # zes_device_properties_t.core is handled by the sysman_device_core_properties_t
    # helper field in sysman_device_properties_info_t, which adds a Uuid string parser.
    # Skipping it here avoids a duplicate "Core" YAML key when zes_device_properties_t
    # is flattened, which would shadow the helper mapping and break UUID parsing.
    ("zes_device_properties_t", "core"): Annotations(ignore=True),
}

# ---------------------------------------------------------------------------
# Parsing helpers
# ---------------------------------------------------------------------------

def read_file(path):
    return Path(path).read_text(encoding="utf-8")


def extract_struct_bodies(text):
    """Return {typedef_name: body_str} for all typedef struct blocks."""
    result = {}
    pat = re.compile(r"typedef\s+struct(?:\s+_?\w+)?\s*\{")
    pos = 0
    while True:
        m = pat.search(text, pos)
        if not m:
            break
        start = m.end()
        depth = 1
        i = start
        while i < len(text) and depth:
            c = text[i]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
            i += 1
        body = text[start : i - 1]
        rest = text[i:].lstrip()
        nm = re.match(r"(\w+)\s*;", rest)
        if nm:
            result[nm.group(1)] = body
        pos = m.start() + 1
    return result


def parse_gen_annotations(raw_line):
    """Parse //gen: directives from a source line."""
    m = re.search(r"//\s*gen:\s*(.+)", raw_line)
    if not m:
        return Annotations()
    ann = Annotations()
    for d in m.group(1).split(","):
        stripped = d.strip()
        parts = stripped.split("=", 1)
        key, value = (parts[0], parts[1]) if len(parts) == 2 else (parts[0], None)
        if key == "ignore" and value is None:
            ann.ignore = True
        elif key == "flatten" and value is None:
            ann.flatten = True
        elif key == "count" and value is not None:
            ann.count = value
        elif key == "key" and value is not None:
            ann.key = value
        else:
            raise ValueError(f"Unknown gen annotation: {stripped!r} in line: {raw_line.strip()!r}")
    return ann


def parse_members(struct_name, body):
    """Parse struct body into Member tuples.

    Member = (type, name, is_ptr, array_size (or None), annotations)
    """
    members = []
    for raw_line in body.splitlines():
        stripped = raw_line.strip()
        if not stripped:
            continue
        # Skip pure comment lines (not gen annotations)
        if stripped.startswith("//") and not re.search(r"//\s*gen:", stripped):
            continue
        if stripped.startswith("/*") or stripped.startswith("*"):
            continue
        # Strip all inline comments (//, ///, //gen:) to get bare code
        code_only = re.split(r"\s*//", stripped)[0].strip()
        if not code_only or not code_only.endswith(";"):
            continue
        # Skip bitfield members
        if re.search(r":\s*\d+\s*;", code_only):
            continue
        if not code_only:
            continue

        # Match "[const] type [**] name [array];"
        m = re.match(
            r"((?:const\s+)?[\w][\w\s]*?)\s+(\*{0,2})\s*(\w+)\s*(\[\w+\])?\s*;",
            code_only,
        )
        if not m:
            continue
        type_str = m.group(1).strip()
        is_ptr = bool(m.group(2).strip())
        name = m.group(3)
        array_size = m.group(4)  # e.g. '[ZES_FAN_TEMP_SPEED_PAIR_COUNT]' or None

        annotations = parse_gen_annotations(raw_line)
        annotations = FIELD_ANNOTATIONS_OVERRIDE.get((struct_name, name), annotations)

        members.append((type_str, name, is_ptr, array_size, annotations))
    return members


def collect_enums(text):
    """Return all typedef enum type names found in text."""
    enums = set()
    for m in re.finditer(r"typedef\s+enum\b[^}]*\}\s+(\w+)\s*;", text, re.DOTALL):
        enums.add(m.group(1))
    return enums


def collect_enum_string_tables(text):
    """Return {type_name: [(str_val, c_name), ...]} for enums

    Only typedef enums enabled with a // gen: enum annotation are included.
    """
    result = {}
    pat = re.compile(r"typedef\s+enum\b[^{]*\{([^}]*)\}\s+(\w+)\s*;([^\n]*)", re.DOTALL)
    for m in pat.finditer(text):
        trailing = m.group(3)
        if not re.search(r"//\s*gen:\s*enum\b", trailing):
            continue
        body = m.group(1)
        type_name = m.group(2)
        pairs = []
        ok = True
        for raw_line in body.splitlines():
            stripped = raw_line.strip()
            if not stripped or stripped.startswith("//") or stripped.startswith("*"):
                continue
            mm = re.match(r"(\w+)", stripped)
            if not mm:
                ok = False
                break
            gk = re.search(r"//\s*gen:\s*key=(\S+)", stripped)
            pairs.append((gk.group(1) if gk else mm.group(1), mm.group(1)))
        if ok and pairs:
            result[type_name] = pairs
    return result


def collect_aliases(text):
    """Return {alias: base_type} for all simple (non-pointer, non-function) typedef aliases."""
    aliases = {}
    for m in re.finditer(
        r"^typedef\s+([\w]+(?:\s*\*)?)\s+(\w+)\s*;", text, re.MULTILINE
    ):
        base = m.group(1).strip()
        alias = m.group(2)
        if alias != base and "(" not in base and "*" not in base:
            aliases[alias] = base
    return aliases


@dataclass
class HeaderParser:
    """Parsed data from one or more C header files."""

    all_structs: dict
    aliases: dict
    enums: set
    enum_str_tables: dict

    @classmethod
    def from_files(cls, paths):
        """Parse all given header files and return a populated HeaderParser."""
        all_structs_raw = {}
        all_text = ""
        for path in paths:
            text = read_file(path)
            all_structs_raw.update(extract_struct_bodies(text))
            all_text += text
        return cls(
            all_structs={name: parse_members(name, body) for name, body in all_structs_raw.items()},
            aliases=collect_aliases(all_text),
            enums=collect_enums(all_text),
            enum_str_tables=collect_enum_string_tables(all_text),
        )

    def resolve_kind(self, name, _seen=None):
        """Return 'bool' | 'float' | 'int' | 'uint' | 'struct' | 'char'."""
        if _seen is None:
            _seen = frozenset()
        if name in _seen:
            return "uint"
        if name == "char":
            return "char"
        if name == "ze_bool_t":
            return "bool"
        if name in {"float", "double"}:
            return "float"
        if name in {"int8_t", "int16_t", "int32_t", "int64_t"}:
            return "int"
        if name in {"uint8_t", "uint16_t", "uint32_t", "uint64_t", "size_t", "uintptr_t"}:
            return "uint"
        if name in self.enums:
            return "uint"
        if name in self.all_structs:
            return "struct"
        if name in self.aliases:
            return self.resolve_kind(self.aliases[name], _seen | {name})
        return "uint"  # unknown: assume flags/enum


# ---------------------------------------------------------------------------
# Code generation helpers
# ---------------------------------------------------------------------------

SKIP_FIELDS = {"stype", "pNext"}

def var_name(struct_name, suffix):
    return re.sub(r"_t$", "", struct_name) + suffix


def fields_var(struct_name):
    return var_name(struct_name, "_fields")


def schema_var(struct_name):
    return var_name(struct_name, "_schema")


def to_yaml_key(name):
    """Convert a C field name to a YAML key string."""
    if "_" in name or name.islower():
        # snake_case -> CamelCase
        return "".join(w.capitalize() for w in name.split("_"))
    # Already CamelCase/mixed: capitalize first letter only
    return name[0].upper() + name[1:]


def detect_count_pairs(members):
    """Return (pairs_dict, count_names_set).

    pairs_dict maps ptr_field_name -> count_field_name for adjacent pairs.
    """
    pairs = {}
    count_names = set()
    for i, (_type_str, name, is_ptr, arr, _ann) in enumerate(members):
        if is_ptr and not arr and i > 0:
            prev_name = members[i - 1][1]
            prev_is_ptr = members[i - 1][2]
            if prev_name == name + "_count" and not prev_is_ptr:
                pairs[name] = prev_name
                count_names.add(prev_name)
    return pairs, count_names


def traverse_from_root(p, root_struct):
    """Depth-first search from root.

    Returns (emit_order, rv_set) where emit_order lists struct names in
    topological order (dependencies before dependents) and rv_set lists "return
    value" structs that need separate RV field arrays.
    NOTE: flattened types are visited for their dependencies but not added to emit_order.
    """
    emit_order = []
    rv_set = set()
    in_order = set()

    def visit_deps(name, flatten_seen):
        """Recurse into members of 'name', scheduling dependencies via visit()."""
        members = p.all_structs.get(name, [])
        _, count_names = detect_count_pairs(members)
        for type_str, field_name, _is_ptr, arr, annotations in members:
            if field_name in SKIP_FIELDS:
                continue
            if field_name in count_names:
                continue
            if annotations.ignore:
                continue
            is_rv = type_str.endswith("_rv_t")
            if arr and not annotations.count:
                continue
            if is_rv:
                rv_set.add(type_str)
            elif annotations.flatten:
                if type_str not in flatten_seen:
                    visit_deps(type_str, flatten_seen | {type_str})
            else:
                visit(type_str)

    def visit(name):
        if name in in_order:
            return
        in_order.add(name)
        visit_deps(name, set())
        emit_order.append(name)  # post-order: after all dependencies

    if root_struct not in p.all_structs:
        sys.exit(f"ERROR: root struct {root_struct!r} not found")

    visit(root_struct)
    return emit_order, rv_set


@dataclass
class SchemaEmitter:
    """Helper for emitting CYAML schema code for a struct and its members."""

    @dataclass
    class FieldCtx:
        """Common context for all member emit methods."""

        yaml_key: str
        type_str: str
        member_path: str
        container: str
        annotations: Annotations = field(default_factory=Annotations)

    parser: HeaderParser
    out: list = field(default_factory=list)

    def _manual_seq(self, ctx, elem_type_str, schema_var, count_path):
        """Emit a manual struct-literal SEQUENCE entry."""
        self.out.append(f'\t{{.key = "{ctx.yaml_key}",')
        self.out.append("\t .value = {.type = CYAML_SEQUENCE,")
        self.out.append("\t           .flags = SYSMAN_NULLABLE_PTR_FLAGS,")
        self.out.append(f"\t           .data_size = sizeof({elem_type_str}),")
        self.out.append(
            f"\t           .sequence = {{.entry = &{schema_var}, .min = 0, .max = CYAML_UNLIMITED}}}},"
        )
        self.out.append(f"\t .data_offset = offsetof({ctx.container}, {ctx.member_path}),")
        self.out.append(f"\t .count_offset = offsetof({ctx.container}, {count_path}),")
        self.out.append("\t .count_size = sizeof(uint32_t)},")

    def _emit_fixed_array(self, ctx, size_expr):
        """Emit a fixed-size array field."""
        if ctx.type_str == "char":
            self.out.append(
                f'\tCYAML_FIELD_STRING("{ctx.yaml_key}", CYAML_FLAG_OPTIONAL, {ctx.container}, {ctx.member_path}, 0),'
            )
        elif ctx.annotations.count:
            sv = schema_var(ctx.type_str) if ctx.type_str in self.parser.all_structs else "uint64_schema"
            self.out.append(
                f'\tCYAML_FIELD_SEQUENCE_COUNT("{ctx.yaml_key}", CYAML_FLAG_OPTIONAL, {ctx.container}, {ctx.member_path},'
                f" {ctx.annotations.count}, &{sv}, 0, {size_expr}),"
            )
        else:
            kind = self.parser.resolve_kind(ctx.type_str)
            if kind == "struct" and ctx.type_str in self.parser.all_structs:
                sv = schema_var(ctx.type_str)
                self.out.append(
                    f'\tCYAML_FIELD_SEQUENCE_FIXED("{ctx.yaml_key}", CYAML_FLAG_OPTIONAL, {ctx.container}, {ctx.member_path},'
                    f" &{sv}, {size_expr}),"
                )
            else:
                self.out.append(
                    f'\tCYAML_FIELD_SEQUENCE_FIXED("{ctx.yaml_key}", CYAML_FLAG_OPTIONAL, {ctx.container}, {ctx.member_path},'
                    f" &uint64_schema, {size_expr}),"
                )

    def _emit_count_seq(self, ctx, count_name, count_path, prefix):
        """Emit a count+pointer sequence field pair."""
        kind = self.parser.resolve_kind(ctx.type_str)
        if kind == "struct" and ctx.type_str in self.parser.all_structs:
            sv = schema_var(ctx.type_str)
            if prefix:
                # Dotted paths (e.g. base.entries_count) are non-standard in macros; use manual form.
                self._manual_seq(ctx, ctx.type_str, sv, count_path)
            else:
                self.out.append(
                    f'\tCYAML_FIELD_SEQUENCE_COUNT("{ctx.yaml_key}", SYSMAN_NULLABLE_PTR_FLAGS, {ctx.container},'
                    f" {ctx.member_path}, {count_name}, &{sv}, 0, CYAML_UNLIMITED),"
                )
        elif ctx.type_str in self.parser.enum_str_tables:
            sv = schema_var(ctx.type_str)
            if prefix:
                self._manual_seq(ctx, ctx.type_str, sv, count_path)
            else:
                self.out.append(
                    f'\tCYAML_FIELD_SEQUENCE_COUNT("{ctx.yaml_key}", SYSMAN_NULLABLE_PTR_FLAGS, {ctx.container},'
                    f" {ctx.member_path}, {count_name}, &{sv}, 0, CYAML_UNLIMITED),"
                )
        elif ctx.type_str == "double":
            self._manual_seq(ctx, "double", "double_schema", count_path)
        else:
            if prefix:
                self._manual_seq(ctx, "uint64_t", "uint64_schema", count_path)
            else:
                self.out.append(
                    f'\tCYAML_FIELD_SEQUENCE_COUNT("{ctx.yaml_key}", SYSMAN_NULLABLE_PTR_FLAGS, {ctx.container},'
                    f" {ctx.member_path}, {count_name}, &uint64_schema, 0, CYAML_UNLIMITED),"
                )

    def _emit_ptr_field(self, ctx):
        """Emit a non-array pointer field."""
        kind = self.parser.resolve_kind(ctx.type_str)
        if kind == "struct" and ctx.type_str in self.parser.all_structs:
            fv = fields_var(ctx.type_str)
            self.out.append(
                f'\tCYAML_FIELD_MAPPING_PTR("{ctx.yaml_key}", SYSMAN_NULLABLE_PTR_FLAGS, {ctx.container}, {ctx.member_path}, {fv}),'
            )
        elif kind == "float":
            self.out.append(
                f'\tCYAML_FIELD_FLOAT_PTR("{ctx.yaml_key}", SYSMAN_NULLABLE_PTR_FLAGS, {ctx.container}, {ctx.member_path}),'
            )
        elif kind == "int":
            self.out.append(
                f'\tCYAML_FIELD_INT_PTR("{ctx.yaml_key}", SYSMAN_NULLABLE_PTR_FLAGS, {ctx.container}, {ctx.member_path}),'
            )
        else:
            self.out.append(
                f'\tCYAML_FIELD_UINT_PTR("{ctx.yaml_key}", SYSMAN_NULLABLE_PTR_FLAGS, {ctx.container}, {ctx.member_path}),'
            )

    def _emit_inline_field(self, ctx, depth):
        """Emit an inline (non-pointer, non-array) field."""
        kind = self.parser.resolve_kind(ctx.type_str)
        if kind == "bool":
            self.out.append(
                f'\tCYAML_FIELD_BOOL("{ctx.yaml_key}", CYAML_FLAG_OPTIONAL, {ctx.container}, {ctx.member_path}),'
            )
        elif kind == "float":
            self.out.append(
                f'\tCYAML_FIELD_FLOAT("{ctx.yaml_key}", CYAML_FLAG_OPTIONAL, {ctx.container}, {ctx.member_path}),'
            )
        elif kind == "int":
            self.out.append(
                f'\tCYAML_FIELD_INT("{ctx.yaml_key}", CYAML_FLAG_OPTIONAL, {ctx.container}, {ctx.member_path}),'
            )
        elif kind == "struct" and ctx.type_str in self.parser.all_structs:
            if ctx.type_str.endswith("_rv_t") or not ctx.annotations.flatten or depth >= 5:
                fv = fields_var(ctx.type_str)
                self.out.append(
                    f'\tCYAML_FIELD_MAPPING("{ctx.yaml_key}", CYAML_FLAG_OPTIONAL, {ctx.container}, {ctx.member_path}, {fv}),'
                )
            else:
                # Flatten: recurse with extended dot-path prefix
                self.emit_members(ctx.container, self.parser.all_structs[ctx.type_str], prefix=f"{ctx.member_path}.", depth=depth + 1)
        else:
            self.out.append(
                f'\tCYAML_FIELD_UINT("{ctx.yaml_key}", CYAML_FLAG_OPTIONAL, {ctx.container}, {ctx.member_path}),'
            )

    def emit_members(self, container, members, prefix="", depth=0):
        """Emit CYAML_FIELD_* entries for all non-skipped members."""
        pairs, count_names = detect_count_pairs(members)

        for type_str, name, is_ptr, arr, annotations in members:
            member_path = f"{prefix}{name}" if prefix else name
            yaml_key = annotations.key if annotations.key else to_yaml_key(name)
            if name in SKIP_FIELDS or name in count_names or annotations.ignore:
                continue

            ctx = SchemaEmitter.FieldCtx(
                yaml_key=yaml_key,
                type_str=type_str,
                member_path=member_path,
                container=container,
                annotations=annotations,
            )

            if arr:
                self._emit_fixed_array(ctx, arr[1:-1])
            elif name in pairs:
                count_name = pairs[name]
                count_path = f"{prefix}{count_name}" if prefix else count_name
                self._emit_count_seq(ctx, count_name, count_path, prefix)
            else:
                if is_ptr:
                    self._emit_ptr_field(ctx)
                else:
                    self._emit_inline_field(ctx, depth)



def emit(out, text):
    """Append lines of a static text block to out, trimming the leading newline."""
    out.extend(text.lstrip("\n").splitlines())


_CYAML_FILE_HEADER = """
/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Code generated by hack/generate-cyaml.py. DO NOT EDIT.
 */

#ifndef SYSMAN_STATE_CYAML_H
#define SYSMAN_STATE_CYAML_H

#include "sysman_state.h"

#include <cyaml/cyaml.h>

"""

_CYAML_FILE_RV_MACRO_BEGIN = """
// Return value schemas (per hierarchy level).
#define RV(T, n) CYAML_FIELD_UINT(#n, CYAML_FLAG_OPTIONAL, T, n)
"""

_CYAML_FILE_SCALAR_SCHEMAS = """
#undef RV

#define SYSMAN_NULLABLE_PTR_FLAGS (CYAML_FLAG_OPTIONAL | CYAML_FLAG_POINTER_NULL_STR)

static const cyaml_schema_value_t double_schema = {CYAML_VALUE_FLOAT(CYAML_FLAG_DEFAULT, double)};
static const cyaml_schema_value_t uint64_schema = {CYAML_VALUE_UINT(CYAML_FLAG_DEFAULT, uint64_t)};
"""

_CYAML_FILE_FOOTER = """
#undef SYSMAN_NULLABLE_PTR_FLAGS

#endif /* SYSMAN_STATE_CYAML_H */
"""


def generate(p, rv_set, emit_order, root_struct):
    out = []
    emitter = SchemaEmitter(parser=p, out=out)

    emit(out, _CYAML_FILE_HEADER)

    # RV field arrays
    emit(out, _CYAML_FILE_RV_MACRO_BEGIN)

    for rv_struct in sorted(rv_set):
        fv = fields_var(rv_struct)
        members = p.all_structs.get(rv_struct, [])
        out.append(f"static const cyaml_schema_field_t {fv}[] = {{")
        for _type_str, name, _is_ptr, _arr, _ann in members:
            out.append(f"\tRV({rv_struct}, {name}),")
        out.append("\tCYAML_FIELD_END};")
        out.append("")

    emit(out, _CYAML_FILE_SCALAR_SCHEMAS)

    # Emit enum schemas
    referenced_enum_str = {
        type_str
        for struct_name in emit_order
        for type_str, _n, is_ptr, _a, _ann in p.all_structs.get(struct_name, [])
        if is_ptr and type_str in p.enum_str_tables
    }
    for enum_name in sorted(referenced_enum_str):
        strvals_var = re.sub(r"_t$", "", enum_name) + "_strvals"
        sv = schema_var(enum_name)
        out.append(f"static const cyaml_strval_t {strvals_var}[] = {{")
        for str_val, c_name in p.enum_str_tables[enum_name]:
            out.append(f'\t{{"{str_val}", {c_name}}},')
        out.append("};")
        out.append(
            f"static const cyaml_schema_value_t {sv} = "
            f"{{CYAML_VALUE_ENUM(CYAML_FLAG_DEFAULT, {enum_name},"
            f" {strvals_var}, CYAML_ARRAY_LEN({strvals_var}))}};"
        )
        out.append("")

    # Emit each schema in topological order
    for struct_name in emit_order:
        members = p.all_structs.get(struct_name, [])
        if not members:
            continue

        fv = fields_var(struct_name)
        sv = schema_var(struct_name)

        out.append(f"static const cyaml_schema_field_t {fv}[] = {{")
        emitter.emit_members(struct_name, members)
        out.append("\tCYAML_FIELD_END};")

        flag = "CYAML_FLAG_POINTER" if struct_name == root_struct else "CYAML_FLAG_DEFAULT"
        out.append(f"static const cyaml_schema_value_t {sv} = {{CYAML_VALUE_MAPPING({flag}, {struct_name}, {fv})}};")
        out.append("")

    emit(out, _CYAML_FILE_FOOTER)

    return "\n".join(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--header-file", action="append", dest="header_files", required=True, metavar="FILE",
                        help="Header file to parse (may be specified multiple times; later files override earlier)")
    parser.add_argument("--output-file", default=None, metavar="FILE",
                        help="Output header file (default: stdout)")
    parser.add_argument("--root-struct", required=True, metavar="TYPE",
                        help="Root struct type name")
    args = parser.parse_args()

    # Parse all header files
    p = HeaderParser.from_files(args.header_files)

    # Discover structs (and their ordering)
    emit_order, rv_set = traverse_from_root(p, args.root_struct)

    code = generate(p, rv_set, emit_order, args.root_struct)

    if args.output_file:
        Path(args.output_file).write_text(code, encoding="utf-8")
        print(f"Written: {args.output_file}", file=sys.stderr)
    else:
        sys.stdout.write(code)
    print(f"Schemas: {len(emit_order)}, RV arrays: {len(rv_set)}", file=sys.stderr)


if __name__ == "__main__":
    main()
