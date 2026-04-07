# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

"""
Command execution helpers.
Handles direct execution, script wrapping, and before/after hooks.
"""

import logging
import os
import shlex
import subprocess
import platform
from typing import Any, Dict

logger = logging.getLogger("CLITestRunner")

_IS_WINDOWS = platform.system() == "Windows"


def _full_binary(binary_path: str) -> str:
    """Return the binary path with .exe suffix (Windows) and shell-safe quoting."""
    if _IS_WINDOWS:
        if not binary_path.endswith('.exe'):
            binary_path = f"{binary_path}.exe"
        # cmd.exe uses double-quote style; shlex.quote produces POSIX quoting which
        # is not understood by cmd.exe, so wrap manually.
        return f'"{binary_path}"'
    return shlex.quote(binary_path)


def _timeout_result(timeout: int) -> Dict[str, Any]:
    return {'return_code': -1, 'stdout': '', 'stderr': f'Timeout after {timeout}s', 'success': False}


def _error_result(err: Exception) -> Dict[str, Any]:
    return {'return_code': -1, 'stdout': '', 'stderr': str(err), 'success': False}


def execute_command(binary_path: str, command: str, timeout: int = 30,
                    background: bool = False) -> Dict[str, Any]:
    """
    Run a binary command and capture output.

    Args:
        binary_path: Path to the binary (binary is prepended to command).
        command:     Arguments passed to the binary.
        timeout:     Seconds before the process is killed.
        background:  If True, start detached and return immediately.

    Returns:
        Dict with keys: return_code, stdout, stderr, success, process (background only).
    """
    full_command = f"{_full_binary(binary_path)} {command}"
    logger.debug(f"Executing: {full_command}")

    env = os.environ.copy()
    try:
        if background:
            process = subprocess.Popen(
                full_command, shell=True,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                text=True, env=env,
            )
            return {'return_code': None, 'stdout': '', 'stderr': '', 'success': True, 'process': process}

        result = subprocess.run(
            full_command, shell=True, capture_output=True,
            text=True, timeout=timeout, env=env,
        )
        return {
            'return_code': result.returncode,
            'stdout': result.stdout,
            'stderr': result.stderr,
            'success': result.returncode == 0,
        }
    except subprocess.TimeoutExpired:
        logger.error(f"Command timeout after {timeout}s: {full_command}")
        return _timeout_result(timeout)
    except Exception as e:
        logger.error(f"Command execution failed: {e}")
        return _error_result(e)


def execute_with_script(binary_path: str, command: str, script: str,
                        timeout: int, resolved_script: str) -> Dict[str, Any]:
    """
    Run a shell script with {command} replaced by the full binary invocation.

    Args:
        binary_path:     Path to the binary.
        command:         Arguments passed to the binary.
        script:          Raw script text (variables already resolved by caller).
        timeout:         Seconds before kill.
        resolved_script: Script text after variable substitution (passed pre-resolved).
    """
    full_command = f"{_full_binary(binary_path)} {command}"
    script_with_command = resolved_script.replace('{command}', full_command)
    logger.debug(f"Executing with script:\n{script_with_command}")

    try:
        result = subprocess.run(
            script_with_command, shell=True, capture_output=True,
            text=True, timeout=timeout, env=os.environ.copy(),
        )
        return {
            'return_code': result.returncode,
            'stdout': result.stdout,
            'stderr': result.stderr,
            'success': result.returncode == 0,
        }
    except subprocess.TimeoutExpired:
        return _timeout_result(timeout)
    except Exception as e:
        return _error_result(e)


def execute_with_hooks(binary_path: str, command: str, hooks: Dict[str, str],
                       timeout: int, resolved_hooks: Dict[str, str]) -> Dict[str, Any]:
    """
    Run a command surrounded by before_test / on_failure / after_test shell hooks.

    Args:
        binary_path:    Path to the binary.
        command:        Arguments passed to the binary.
        hooks:          Raw hooks dict (keys: before_test, after_test, on_failure).
        timeout:        Seconds before kill.
        resolved_hooks: Hooks dict after variable substitution (passed pre-resolved).
    """
    env = os.environ.copy()

    def _run_hook(name: str) -> None:
        snippet = resolved_hooks.get(name, '')
        if snippet:
            logger.debug(f"Executing {name} hook")
            try:
                subprocess.run(snippet, shell=True, env=env, timeout=timeout)
            except Exception as e:
                logger.error(f"{name} hook failed: {e}")

    _run_hook('before_test')

    full_command = f"{_full_binary(binary_path)} {command}"
    try:
        result = subprocess.run(
            full_command, shell=True, capture_output=True,
            text=True, timeout=timeout, env=env,
        )
        test_result = {
            'return_code': result.returncode,
            'stdout': result.stdout,
            'stderr': result.stderr,
            'success': result.returncode == 0,
        }
        if not test_result['success']:
            _run_hook('on_failure')
    except subprocess.TimeoutExpired:
        test_result = _timeout_result(timeout)
        _run_hook('on_failure')
    except Exception as e:
        test_result = _error_result(e)
        _run_hook('on_failure')

    _run_hook('after_test')
    return test_result
