# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

"""
Data models for the CLI test runner.
Defines all dataclasses and enums shared across modules.
"""

from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, List, Optional


class TestType(Enum):
    """Test execution types."""
    COMMAND = "command"
    WORKFLOW = "workflow"


class StepType(Enum):
    """Workflow step types."""
    NORMAL = "normal"
    BACKGROUND = "background"
    CLEANUP = "cleanup"
    OPTIONAL = "optional"
    CRITICAL = "critical"
    RECOVERY = "recovery"


@dataclass
class WorkflowStep:
    """Represents a single step in a workflow."""
    name: str
    command: str
    description: str = ""
    step_type: StepType = StepType.NORMAL
    timeout: Optional[int] = None
    output_format: str = "plaintext"
    expect: Dict[str, Any] = field(default_factory=dict)
    condition: Optional[str] = None
    store_result: Optional[str] = None
    store_output_as: Optional[str] = None
    background_timeout: Optional[int] = None
    cleanup_step: bool = False
    optional: bool = False
    critical: bool = False
    run_on_failure: bool = False


@dataclass
class StepResult:
    """Represents the result of a single workflow step."""
    step_name: str
    passed: bool
    message: str = ""
    duration: float = 0.0
    return_code: int = 0
    stdout: str = ""
    stderr: str = ""
    skipped: bool = False


@dataclass
class TestResult:
    """Represents the result of a single test execution."""
    test_name: str
    passed: bool
    message: str = ""
    duration: float = 0.0
    details: Dict[str, Any] = field(default_factory=dict)
    step_results: List[StepResult] = field(default_factory=list)
    # Full command line executed (binary + args); populated by run_single_command_test
    command: str = ""
    # Snapshot of the expect: block from the test config; used by issue reporter
    expect_config: Dict[str, Any] = field(default_factory=dict)

    def __str__(self) -> str:
        status = "PASS" if self.passed else "FAIL"
        return f"[{status}] {self.test_name}: {self.message}"


@dataclass
class RetryConfig:
    """Configuration for test/step retry behavior."""
    max_attempts: int = 1
    backoff: str = "linear"  # linear, exponential, fixed
    base_delay: float = 1.0
    retry_on_step_failure: bool = False
