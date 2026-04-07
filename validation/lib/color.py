# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

"""
Terminal color helpers using colorama.
Falls back gracefully to plain strings if colorama is not installed.
"""

try:
    from colorama import Fore, Style, init as colorama_init
    colorama_init(autoreset=True)
    _COLOR = True
except ImportError:
    _COLOR = False


def green(s: str) -> str:
    return f"{Fore.GREEN}{s}{Style.RESET_ALL}" if _COLOR else s


def red(s: str) -> str:
    return f"{Fore.RED}{s}{Style.RESET_ALL}" if _COLOR else s


def yellow(s: str) -> str:
    return f"{Fore.YELLOW}{s}{Style.RESET_ALL}" if _COLOR else s


def cyan(s: str) -> str:
    return f"{Fore.CYAN}{s}{Style.RESET_ALL}" if _COLOR else s


def bold(s: str) -> str:
    return f"{Style.BRIGHT}{s}{Style.RESET_ALL}" if _COLOR else s
