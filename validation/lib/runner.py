# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

"""
CLITestRunner — main coordinator class.

Ties together executor, validators, resolver, and models to run
test suites defined in YAML configuration files.
"""

import itertools
import json
import logging
import os
import platform
import subprocess
import sys
import time
import yaml
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from .models import (
    TestType, StepType, WorkflowStep,
    TestResult, StepResult,
)
from .color import green, red, yellow, bold
from .validators import validate_return_code, validate_plaintext, validate_json_output
from .executor import execute_command, execute_with_script, execute_with_hooks, _full_binary
from .resolver import DependencyResolver


class CLITestRunner:
    """Main test runner for CLI validation with workflow and dependency support."""

    def __init__(self, binary_path: str = "xpu-smi", verbose: bool = False,
                 platform_type: str = "BMG", max_workers: int = 1,
                 log_file: Optional[str] = None):
        self.binary_path = binary_path
        self.verbose = verbose
        self.is_windows = platform.system() == "Windows"
        self.os_name = "Windows" if self.is_windows else "Linux"
        self.platform_type = platform_type
        self.max_workers = max_workers
        self.log_file = log_file
        self.logger = self._setup_logger()
        self.results: List[TestResult] = []
        self.workflow_context: Dict[str, Any] = {}

    # ------------------------------------------------------------------
    # Setup
    # ------------------------------------------------------------------

    def _setup_logger(self) -> logging.Logger:
        logger = logging.getLogger("CLITestRunner")
        logger.setLevel(logging.DEBUG if self.verbose else logging.INFO)

        if not logger.handlers:
            console = logging.StreamHandler(sys.stdout)
            console.setFormatter(logging.Formatter('%(message)s'))
            logger.addHandler(console)

        if self.log_file and not any(isinstance(h, logging.FileHandler) for h in logger.handlers):
            fh = logging.FileHandler(self.log_file, encoding='utf-8')
            fh.setLevel(logging.DEBUG)
            fh.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))
            logger.addHandler(fh)

        return logger

    def load_test_config(self, config_path: Union[str, Path]) -> Dict[str, Any]:
        config_path = Path(config_path)
        self.logger.info(f"Loading test configuration from {config_path}")
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                return yaml.safe_load(f)
        except Exception as e:
            self.logger.error(f"Failed to load config: {e}")
            raise

    # ------------------------------------------------------------------
    # Variable resolution
    # ------------------------------------------------------------------

    def resolve_variables(self, text: str, variables: Dict[str, Any]) -> str:
        """Substitute OS-specific {var} placeholders in text.

        Unknown keys (e.g. {command}, shell $$ expansions) are left intact so
        they can be resolved later by the shell or by the script execution path.
        """
        if not variables:
            return text
        resolved = {
            name: (val.get(self.os_name, val.get('default', '')) if isinstance(val, dict) else val)
            for name, val in variables.items()
        }

        class _PassThrough(dict):
            def __missing__(self, key):
                return '{' + key + '}'

        return text.format_map(_PassThrough(resolved))

    # ------------------------------------------------------------------
    # Delegation wrappers (keep call sites inside this class unchanged)
    # ------------------------------------------------------------------

    def execute_command(self, command: str, timeout: int = 30,
                        background: bool = False) -> Dict[str, Any]:
        return execute_command(self.binary_path, command, timeout, background)

    def validate_return_code(self, actual: int, expected) -> bool:
        return validate_return_code(actual, expected)

    def validate_plaintext(self, output: str, expectations: Dict[str, Any]):
        return validate_plaintext(output, expectations)

    def validate_json_output(self, output: str, expectations: Dict[str, Any]):
        return validate_json_output(output, expectations)

    def _execute_with_script(self, command: str, script: str, timeout: int,
                             variables: Dict[str, Any]) -> Dict[str, Any]:
        resolved = self.resolve_variables(script, variables)
        return execute_with_script(self.binary_path, command, script, timeout, resolved)

    def _execute_with_hooks(self, command: str, hooks: Dict[str, str], timeout: int,
                            variables: Dict[str, Any]) -> Dict[str, Any]:
        resolved = {k: self.resolve_variables(v, variables) for k, v in hooks.items() if v}
        return execute_with_hooks(self.binary_path, command, hooks, timeout, resolved)

    # ------------------------------------------------------------------
    # Condition evaluation
    # ------------------------------------------------------------------

    def evaluate_condition(self, condition: str, context: Dict[str, Any]) -> bool:
        try:
            return bool(eval(condition, {"__builtins__": {}}, context))
        except Exception as e:
            self.logger.error(f"Failed to evaluate condition '{condition}': {e}")
            return False

    # ------------------------------------------------------------------
    # Workflow execution
    # ------------------------------------------------------------------

    def run_workflow_step(self, step: WorkflowStep, workflow_context: Dict[str, Any],
                          global_settings: Dict[str, Any], variables: Dict[str, Any]) -> StepResult:
        start_time = time.time()

        if step.condition and not self.evaluate_condition(step.condition, workflow_context):
            return StepResult(step_name=step.name, passed=True,
                              message="Skipped due to condition", skipped=True,
                              duration=time.time() - start_time)

        self.logger.info(f"  Running step: {step.name}")
        if step.description:
            self.logger.debug(f"    Description: {step.description}")

        command = self.resolve_variables(step.command, variables)
        timeout = step.timeout or global_settings.get('timeout', 30)

        if step.step_type == StepType.BACKGROUND:
            result = self.execute_command(command, timeout, background=True)
            if 'process' in result:
                workflow_context[f"_bg_process_{step.name}"] = result['process']
            expected_rc = step.expect.get('startup_return_code', 0)
            success = (
                validate_return_code(result['return_code'], expected_rc)
                if result.get('return_code') is not None
                else True
            )
            step_result = StepResult(
                step_name=step.name, passed=success,
                message="Background step started" if success else "Failed to start background step",
                duration=time.time() - start_time,
                return_code=result.get('return_code', 0),
                stdout=result.get('stdout', ''), stderr=result.get('stderr', ''),
            )
        else:
            result = self.execute_command(command, timeout)
            expected_rc = step.expect.get('return_code', 0)
            if not validate_return_code(result['return_code'], expected_rc):
                step_result = StepResult(
                    step_name=step.name, passed=False,
                    message=f"Return code mismatch: expected {expected_rc}, got {result['return_code']}",
                    duration=time.time() - start_time,
                    return_code=result['return_code'],
                    stdout=result['stdout'], stderr=result['stderr'],
                )
            else:
                if step.output_format == 'json':
                    success, message = validate_json_output(result['stdout'], step.expect)
                else:
                    success, message = validate_plaintext(result['stdout'], step.expect)
                step_result = StepResult(
                    step_name=step.name, passed=success, message=message,
                    duration=time.time() - start_time,
                    return_code=result['return_code'],
                    stdout=result['stdout'], stderr=result['stderr'],
                )

        if step.store_result:
            workflow_context[step.store_result] = step_result.return_code
        if step.store_output_as:
            workflow_context[step.store_output_as] = step_result.stdout

        return step_result

    def run_workflow(self, test_config: Dict[str, Any],
                     global_settings: Dict[str, Any]) -> TestResult:
        test_name = test_config.get('name', 'unnamed_workflow')
        description = test_config.get('description', '')
        steps_config = test_config.get('steps', [])
        variables = test_config.get('variables', {})
        environment = test_config.get('environment', {})
        continue_on_step_failure = test_config.get('continue_on_step_failure', False)

        self.logger.info(f"Running workflow: {test_name}")
        if description:
            self.logger.info(f"  Description: {description}")

        workflow_context = {
            k: (self.resolve_variables(v, variables) if isinstance(v, str) else v)
            for k, v in environment.items()
        }

        steps = []
        for cfg in steps_config:
            step = WorkflowStep(
                name=cfg.get('name', 'unnamed_step'),
                command=cfg.get('command', ''),
                description=cfg.get('description', ''),
                timeout=cfg.get('timeout'),
                output_format=cfg.get('output_format', 'plaintext'),
                expect=cfg.get('expect', {}),
                condition=cfg.get('condition'),
                store_result=cfg.get('store_result'),
                store_output_as=cfg.get('store_output_as'),
                background_timeout=cfg.get('background_timeout'),
                cleanup_step=cfg.get('cleanup_step', False),
                optional=cfg.get('optional', False),
                critical=cfg.get('critical', False),
                run_on_failure=cfg.get('run_on_failure', False),
            )
            if cfg.get('background', False):
                step.step_type = StepType.BACKGROUND
            elif step.cleanup_step:
                step.step_type = StepType.CLEANUP
            elif step.optional:
                step.step_type = StepType.OPTIONAL
            elif step.critical:
                step.step_type = StepType.CRITICAL
            elif step.run_on_failure:
                step.step_type = StepType.RECOVERY
            steps.append(step)

        step_results: List[StepResult] = []
        workflow_failed = False
        start_time = time.time()

        try:
            for step in steps:
                should_run = not (
                    (step.run_on_failure and not workflow_failed) or
                    (workflow_failed and not (step.cleanup_step or step.run_on_failure))
                )
                if not should_run:
                    step_results.append(StepResult(step_name=step.name, passed=True,
                                                    message="Skipped", skipped=True))
                    continue

                step_result = self.run_workflow_step(step, workflow_context, global_settings, variables)
                step_results.append(step_result)

                if step_result.passed:
                    self.logger.info(green("    [PASS]") + f" {step.name}: {step_result.message}")
                else:
                    self.logger.error(red("    [FAIL]") + f" {step.name}: {step_result.message}")
                    if self.verbose:
                        self.logger.debug(f"      Stdout: {step_result.stdout[:200]}")
                        self.logger.debug(f"      Stderr: {step_result.stderr[:200]}")

                if not step_result.passed:
                    if step.critical:
                        workflow_failed = True
                        break
                    elif not step.optional:
                        workflow_failed = True
                        if not continue_on_step_failure:
                            break
        finally:
            for key, process in workflow_context.items():
                if key.startswith('_bg_process_') and hasattr(process, 'terminate'):
                    try:
                        process.terminate()
                        process.wait(timeout=5)
                    except Exception as e:
                        self.logger.warning(f"Failed to cleanup background process: {e}")

        duration = time.time() - start_time
        total = len([r for r in step_results if not r.skipped])
        passed_count = len([r for r in step_results if r.passed and not r.skipped])
        failed_count = total - passed_count

        if workflow_failed:
            return TestResult(test_name=test_name, passed=False, duration=duration,
                              message=f"Workflow failed: {failed_count}/{total} steps failed",
                              step_results=step_results)
        return TestResult(test_name=test_name, passed=True, duration=duration,
                          message=f"Workflow completed: {passed_count}/{total} steps passed",
                          step_results=step_results)

    # ------------------------------------------------------------------
    # Single-command test execution
    # ------------------------------------------------------------------

    def run_single_command_test(self, test_config: Dict[str, Any],
                                global_settings: Dict[str, Any]) -> TestResult:
        test_name = test_config.get('name', 'unnamed_test')
        description = test_config.get('description', '')
        command = test_config.get('command', '')
        output_format = test_config.get('output_format', 'plaintext')
        expectations = test_config.get('expect', {})
        timeout = test_config.get('timeout', global_settings.get('timeout', 30))
        variables = test_config.get('variables', {})

        # OS / platform filters
        os_filter = test_config.get('os')
        if os_filter:
            allowed = [os_filter] if isinstance(os_filter, str) else os_filter
            if self.os_name not in allowed:
                self.logger.info(f"Skipping {test_name} (OS filter: {allowed})")
                return TestResult(test_name, True, f"Skipped (OS: {self.os_name} not supported)")

        platform_filter = test_config.get('platform')
        if platform_filter:
            allowed = [platform_filter] if isinstance(platform_filter, str) else platform_filter
            if self.platform_type not in allowed:
                self.logger.info(f"Skipping {test_name} (platform filter: {allowed})")
                return TestResult(test_name, True, f"Skipped (Platform: {self.platform_type} not supported)")

        self.logger.info(f"Running test: {test_name}")
        if description:
            self.logger.info(f"  Description: {description}")

        if variables:
            command = self.resolve_variables(command, variables)

        start_time = time.time()

        wrapper = test_config.get('wrapper')
        script = test_config.get('script')
        hooks = test_config.get('hooks')

        if hooks:
            result = self._execute_with_hooks(command, hooks, timeout, variables)
        elif script:
            result = self._execute_with_script(command, script, timeout, variables)
        elif wrapper:
            wrapped = self.resolve_variables(wrapper, variables).replace(
                '{command}', f"{_full_binary(self.binary_path)} {command}"
            )
            r = subprocess.run(wrapped, shell=True, capture_output=True, text=True,
                               timeout=timeout, env=os.environ.copy())
            result = {'return_code': r.returncode, 'stdout': r.stdout,
                      'stderr': r.stderr, 'success': r.returncode == 0}
        else:
            result = self.execute_command(command, timeout)

        duration = time.time() - start_time

        expected_rc = expectations.get('return_code', 0)
        if not validate_return_code(result['return_code'], expected_rc):
            msg = f"Return code mismatch: expected {expected_rc}, got {result['return_code']}"
            if result['stderr']:
                msg += f"\nStderr: {result['stderr'][:200]}"
            return TestResult(test_name, False, msg, duration, result)

        if output_format == 'json':
            success, message = validate_json_output(result['stdout'], expectations)
        else:
            success, message = validate_plaintext(result['stdout'], expectations)

        if 'stdout_or_stderr_contains' in expectations:
            combined = result['stdout'] + result['stderr']
            for pattern in expectations['stdout_or_stderr_contains']:
                if pattern not in combined:
                    success = False
                    message = f"Expected pattern not in output: '{pattern}'"
                    break

        return TestResult(test_name, success, message, duration, result)

    def run_test(self, test_config: Dict[str, Any],
                 global_settings: Dict[str, Any]) -> TestResult:
        max_retries = test_config.get('max_retries', global_settings.get('max_retries', 0))
        result = self._run_test_once(test_config, global_settings)
        for attempt in range(1, max_retries + 1):
            if result.passed:
                break
            self.logger.warning(
                f"Retrying {test_config.get('name', 'unnamed')} "
                f"(attempt {attempt}/{max_retries})"
            )
            result = self._run_test_once(test_config, global_settings)
        return result

    def _run_test_once(self, test_config: Dict[str, Any],
                      global_settings: Dict[str, Any]) -> TestResult:
        if TestType(test_config.get('type', 'command')) == TestType.WORKFLOW:
            return self.run_workflow(test_config, global_settings)
        return self.run_single_command_test(test_config, global_settings)

    # ------------------------------------------------------------------
    # Parameterization
    # ------------------------------------------------------------------

    def _auto_discover_device_ids(self) -> List[int]:
        try:
            result = self.execute_command("discovery -j", timeout=10)
            if result['success'] and result['stdout']:
                data = json.loads(result['stdout'])
                return sorted(
                    d['device_id'] for d in data.get('device_list', []) if 'device_id' in d
                )
        except Exception as e:
            self.logger.debug(f"Auto-discovery failed: {e}")
        return []

    def expand_parameterized_test(self, test_config: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Expand a parameterized test into one instance per parameter combination."""
        if 'parameters' not in test_config or 'command_template' not in test_config:
            return [test_config]

        parameters = test_config['parameters']
        command_template = test_config['command_template']
        base_name = test_config.get('name', 'unnamed_test')
        is_parallel = test_config.get('parallel', False)

        param_names = list(parameters.keys())
        param_value_lists = []

        for name, spec in parameters.items():
            if isinstance(spec, list):
                param_value_lists.append(spec)
            elif isinstance(spec, dict):
                ptype = spec.get('type')
                if ptype == 'range':
                    param_value_lists.append(
                        list(range(spec.get('start', 0), spec.get('end', 1), spec.get('step', 1)))
                    )
                elif ptype == 'auto_discover_devices':
                    ids = self._auto_discover_device_ids()
                    if not ids:
                        self.logger.warning(f"No devices found for auto-discovery in '{base_name}'")
                        ids = [0]
                    param_value_lists.append(ids)
                else:
                    self.logger.warning(f"Unknown parameter type in '{base_name}': {spec}")
                    param_value_lists.append([None])
            else:
                param_value_lists.append([spec])

        parallel_group_id = f"parallel_group_{base_name}" if is_parallel else None
        expanded = []

        for combo in itertools.product(*param_value_lists):
            mapping = dict(zip(param_names, combo))
            try:
                if test_config.get('type') == 'workflow':
                    formatted = test_config.copy()
                    formatted['steps'] = [
                        {**s, 'command': s['command'].format(**mapping)} if 'command' in s else s
                        for s in test_config.get('steps', [])
                    ]
                else:
                    formatted = test_config.copy()
                    formatted['command'] = command_template.format(**mapping)
            except KeyError as e:
                self.logger.error(f"Missing parameter in template for '{base_name}': {e}")
                continue

            suffix = '_'.join(f"{k}={v}" for k, v in mapping.items())
            formatted['name'] = f"{base_name}[{suffix}]"
            formatted.pop('command_template', None)
            formatted.pop('parameters', None)
            formatted.pop('parallel', None)
            formatted['_parameter_values'] = mapping
            if parallel_group_id:
                formatted['_parallel_group'] = parallel_group_id
            expanded.append(formatted)

        return expanded

    # ------------------------------------------------------------------
    # Suite execution
    # ------------------------------------------------------------------

    def run_test_suite(self, config: Dict[str, Any],
                       filter_tags: Optional[List[str]] = None) -> List[TestResult]:
        """Run all tests in a test suite with dependency resolution."""
        test_suite = config.get('test_suite', {})
        suite_name = test_suite.get('name', 'Unknown Suite')
        global_settings = test_suite.get('settings', {})
        tests = test_suite.get('tests', [])

        # Expand parameterized tests
        tests = [t for raw in tests for t in self.expand_parameterized_test(raw)]

        self.logger.info(f"=== Running Test Suite: {suite_name} ===")
        self.logger.info(f"Total tests defined: {len(tests)}")

        if filter_tags:
            tests = [t for t in tests if any(tag in t.get('tags', []) for tag in filter_tags)]
            self.logger.info(f"Filtered to {len(tests)} tests with tags: {filter_tags}")

        continue_on_failure = global_settings.get('continue_on_failure', True)

        try:
            execution_order = DependencyResolver(tests).resolve_execution_order()
        except ValueError as e:
            self.logger.error(f"Dependency resolution failed: {e}")
            return [TestResult("dependency_resolution", False, str(e))]

        # Group parallel variants together
        parallel_groups: Dict[str, List] = {}
        test_by_name = {t['name']: t for t in tests}

        for name in execution_order:
            t = test_by_name[name]
            gid = t.get('_parallel_group')
            if gid:
                parallel_groups.setdefault(gid, []).append(t)

        execution_plan = []
        processed_groups: set = set()
        for name in execution_order:
            t = test_by_name[name]
            gid = t.get('_parallel_group')
            if gid and gid not in processed_groups:
                execution_plan.append(('parallel_group', parallel_groups[gid]))
                processed_groups.add(gid)
            elif not gid:
                execution_plan.append(('single', t))

        results: List[TestResult] = []
        failed_tests: set = set()

        for item_type, item in execution_plan:
            if item_type == 'parallel_group':
                group_tests = item
                deps_failed = any(
                    dep in failed_tests
                    for t in group_tests
                    for dep in ([t.get('depends_on')] if isinstance(t.get('depends_on'), str)
                                else t.get('depends_on', []))
                )
                if deps_failed:
                    for t in group_tests:
                        results.append(TestResult(t['name'], True, "Skipped due to failed dependency"))
                        failed_tests.add(t['name'])
                else:
                    group_name = group_tests[0].get('name', '').split('[')[0]
                    self.logger.info(f"\n=== Parallel group: {group_name} ({len(group_tests)} variants) ===")
                    workers = self.max_workers if self.max_workers > 1 else min(len(group_tests), 4)
                    saved = self.max_workers
                    self.max_workers = workers
                    group_results = self._run_tests_parallel(group_tests, global_settings, continue_on_failure)
                    self.max_workers = saved
                    results.extend(group_results)
                    failed_tests.update(r.test_name for r in group_results if not r.passed)
            else:
                test_config = item
                test_name = test_config['name']
                deps = test_config.get('depends_on', [])
                if isinstance(deps, str):
                    deps = [deps]
                if any(dep in failed_tests for dep in deps):
                    results.append(TestResult(test_name, True, "Skipped due to failed dependency"))
                    failed_tests.add(test_name)
                else:
                    self.logger.info(f"\n--- Test {len(results)+1}/{len(tests)} ---")
                    result = self.run_test(test_config, global_settings)
                    results.append(result)
                    if result.passed:
                        self.logger.info(green("[PASS]") + f" {result.test_name}: {result.message}")
                    else:
                        self.logger.error(red("[FAIL]") + f" {result.test_name}: {result.message}")
                        failed_tests.add(test_name)
                        if self.verbose and result.details:
                            self.logger.debug(f"  Stdout: {result.details.get('stdout', '')[:500]}")
                            self.logger.debug(f"  Stderr: {result.details.get('stderr', '')[:500]}")
                        if not continue_on_failure:
                            self.logger.warning(yellow("Stopping test suite due to failure"))
                            break

        return results

    def _run_tests_parallel(self, tests: List[Dict[str, Any]],
                            global_settings: Dict[str, Any],
                            continue_on_failure: bool) -> List[TestResult]:
        self.logger.info(f"Running tests in parallel (max workers: {self.max_workers})")
        results: List[Optional[TestResult]] = [None] * len(tests)
        futures_to_index = {}

        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            for i, test_config in enumerate(tests):
                futures_to_index[executor.submit(self.run_test, test_config, global_settings)] = i

            completed = 0
            for future in as_completed(futures_to_index):
                completed += 1
                idx = futures_to_index[future]
                test_name = tests[idx].get('name', 'unnamed_test')
                try:
                    result = future.result()
                    results[idx] = result
                    if result.passed:
                        self.logger.info(green(f"[{completed}/{len(tests)}] [PASS]") + f" {test_name}")
                    else:
                        self.logger.error(red(f"[{completed}/{len(tests)}] [FAIL]") + f" {test_name}")
                    if not result.passed and not continue_on_failure:
                        self.logger.warning(yellow("Test failed - cancelling remaining tests"))
                        for f in futures_to_index:
                            if not f.done():
                                f.cancel()
                        break
                except Exception as e:
                    self.logger.error(f"Test {test_name} raised exception: {e}")
                    results[idx] = TestResult(test_name, False, f"Exception: {e}")

        return [r for r in results if r is not None]

    # ------------------------------------------------------------------
    # Summary
    # ------------------------------------------------------------------

    def print_summary(self, results: List[TestResult]) -> None:
        total = len(results)
        passed = sum(1 for r in results if r.passed)
        failed = total - passed
        rate = (passed / total * 100) if total > 0 else 0

        self.logger.info("\n" + bold("=" * 60))
        self.logger.info(bold("TEST SUMMARY"))
        self.logger.info(bold("=" * 60))
        self.logger.info(f"Total:   {total}")
        self.logger.info(green(f"Passed:  {passed}") if passed else f"Passed:  {passed}")
        self.logger.info(red(f"Failed:  {failed}") if failed else f"Failed:  {failed}")
        self.logger.info(f"Rate:    {rate:.1f}%")

        if failed > 0:
            self.logger.info("\n" + bold("Failed tests:"))
            for result in results:
                if not result.passed:
                    self.logger.info(red(f"  ✗ {result.test_name}") + f": {result.message}")
                    for step in [s for s in result.step_results if not s.passed and not s.skipped]:
                        self.logger.info(f"    └─ {yellow(step.step_name)}: {step.message}")

        self.logger.info(bold("=" * 60))
        if self.log_file:
            self.logger.info(f"Log file: {self.log_file}")
