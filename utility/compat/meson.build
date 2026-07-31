# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Compatibility shim for std::format / {fmt}
#
# fmt_dep is declared in the root meson.build and is either an empty
# dependency (when the compiler has a full std::format) or the {fmt}
# library (on GCC < 13 and similar environments).

compat_inc = include_directories('.')

compat_dep = declare_dependency(
    include_directories: [compat_inc],
    dependencies: fmt_dep.found() ? [fmt_dep] : [],
)
