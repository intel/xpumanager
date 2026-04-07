# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT

"""
Dependency resolution for test suites.
Uses a topological sort (Kahn's algorithm) to determine execution order.
"""

from collections import defaultdict, deque
from typing import Any, Dict, List


class DependencyResolver:
    """Resolves test dependencies and produces a valid execution order."""

    def __init__(self, tests: List[Dict[str, Any]]):
        self.tests = {test['name']: test for test in tests}
        self._deps = self._build_graph()

    def _build_graph(self) -> Dict[str, List[str]]:
        graph = {}
        for name, cfg in self.tests.items():
            deps = cfg.get('depends_on', [])
            graph[name] = [deps] if isinstance(deps, str) else list(deps)
        return graph

    def resolve_execution_order(self) -> List[str]:
        """
        Return test names in dependency-safe execution order.

        Raises:
            ValueError: On unknown dependency or circular dependency.
        """
        in_degree: Dict[str, int] = defaultdict(int)
        adjacency: Dict[str, List[str]] = defaultdict(list)

        for name in self.tests:
            in_degree[name] = in_degree.get(name, 0)

        for name, deps in self._deps.items():
            for dep in deps:
                if dep not in self.tests:
                    raise ValueError(f"Test '{name}' depends on unknown test '{dep}'")
                adjacency[dep].append(name)
                in_degree[name] += 1

        queue = deque(name for name, deg in in_degree.items() if deg == 0)
        order: List[str] = []

        while queue:
            current = queue.popleft()
            order.append(current)
            for neighbour in adjacency[current]:
                in_degree[neighbour] -= 1
                if in_degree[neighbour] == 0:
                    queue.append(neighbour)

        if len(order) != len(self.tests):
            cycle = set(self.tests) - set(order)
            raise ValueError(f"Circular dependency detected involving tests: {cycle}")

        return order
