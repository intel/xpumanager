#!/usr/bin/awk -f
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#
# Print the targets of a Makefile
#
# A target is documented by the comment lines immediately preceding it, e.g.
#
#     .PHONY: build
#     # Build everything
#     build:
#
# and sections are started by banner comments of the form '# --- Name ---'.
# Internal notes not meant for the help output can be placed above the '.PHONY' line, e.g.
#
#     # Remember to keep in sync with CI
#     .PHONY: lint
#     # Run the linter
#     lint:

BEGIN {
    FS = ":"
    BOLD = "\033[1m"
    TARGET = "\033[1;36m"
    RESET = "\033[0m"
    WIDTH = 30
}

# Section header
/^# --- / {
    gsub(/^# --- | *-+ *$/, "")
    printf "\n%s%s:%s\n", BOLD, $0, RESET
    clear_doc()
    next
}

# Comment line(s)
/^#/ {
    sub(/^# ?/, "")
    doc[++doc_lines] = $0
    next
}

# Target (but not a variable assignment): print it with its documentation
/^[a-zA-Z0-9][^ =]*:/ {
    if (doc_lines) {
        printf "  %s%-*s%s %s\n", TARGET, WIDTH, $1, RESET, doc[1]
        for (i = 2; i <= doc_lines; i++)
            printf "  %-*s %s\n", WIDTH, "", doc[i]
    } else {
        printf "  %s%s%s\n", TARGET, $1, RESET
    }
    clear_doc()
    next
}

# Anything else (blank lines, variables...) ends a documentation block
{
    clear_doc()
}

function clear_doc() {
    doc_lines = 0
}
