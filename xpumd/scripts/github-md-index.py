#!/usr/bin/python3
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
"""
Helper script to create GitHub Markdown doc index from all the headings
in the file (heading = line starting with '#', with each additional '#'
increasing the index entry indent level).

Usage:
    github-md-index.py <Markdown file> [skip level]

Optional "skip level" argument specified which headings are skipped.
E.g. if document has (single) top level title marked with '#', specify
"1" to skip that, and one level of index indenting.
"""

import os
import sys
from typing import Never, TextIO


def error_exit(msg: str) -> Never:
    "show usage, given error message and exit"
    sys.stderr.write(__doc__)
    sys.stderr.write(f"ERROR: {msg}!")
    sys.exit(1)

def create_translation() -> dict:
    "create translation table for removing special chars from GitHub doc links"
    to_remove = {}
    for char in "?!:@.,;()[]\"'/\\*_~|$+=<>":
        to_remove[ord(char)] = None
    return to_remove

def output_index(mdfile: TextIO, skip: int) -> Never:
    "output index for given Markdown file"
    to_remove = create_translation()
    anchors = {}
    literal = False
    for line in mdfile:
        if line.startswith("```"):
            literal = not literal
            continue
        if literal:
            continue
        if not line.startswith("#"):
            continue
        # handle title
        depth = line.index(' ')
        title = line[depth:].strip()
        link = title.lower().replace(" ", "-")
        if link.find("--") >= 0:
            print(f"ERROR: multiple successive spaces in '{link}' for\n{line}!")
            continue
        link = link.translate(to_remove)
        count = anchors.get(link, 0)
        anchors[link] = count + 1
        if depth <= skip:
            continue
        # further anchors with same name?
        if count:
            link = f"{link}-{count}"
        print(f"{' '*2*(depth-skip-1)}* [{title}](#{link})")

def main() -> Never:
    "check args & call index generation"
    args = sys.argv[1:]
    if len(args) < 1 or len(args) > 2:
        error_exit("invalid number of args")
    path = args[0]
    if not os.path.exists(path):
        error_exit(f"specified file does not exist: {path}")
    if len(args) > 1:
        try:
            skip = int(args[1])
        except ValueError:
            error_exit(f"invalid skip level value: {args[1]}")
    else:
        skip = 0
    with open(path, encoding="utf-8") as mdfile:
        output_index(mdfile, skip)

if __name__ == "__main__":
    main()
