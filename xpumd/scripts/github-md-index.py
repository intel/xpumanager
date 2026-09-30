#!/usr/bin/python3
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
"""
Helper script to create GitHub Markdown doc index

Usage: github-md-index.py < README.md
"""

import sys

# create translation table for removing special chars from GitHub doc links
to_remove = {}
for char in "?!:@.,;()[]\"'/\\*_~|$+=<>":
    to_remove[ord(char)] = None

anchors = {}
literal = False
for line in sys.stdin.readlines():
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
    # further anchors with same name?
    if count:
        link = f"{link}-{count}"
    print(f"{' '*(depth-1)*2}* [{title}](#{link})")
