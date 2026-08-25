"""PlatformIO test runner for geoscout's host-side suites.

The suites are plain C++ programs with their own main() and a tiny assertion
header, so that `make test` works with nothing installed but a compiler. This
adapter teaches `pio test` to read their output, so both entry points run the
exact same binaries.
"""

import re

import click

from platformio.test.result import TestCase, TestCaseSource, TestStatus
from platformio.test.runners.base import TestRunnerBase

SUMMARY_RE = re.compile(r"^(?P<status>PASS|FAIL)\s+(?P<name>\S+)\s+(?P<detail>.*)$")
DETAIL_RE = re.compile(r"^\s+FAIL\s+(?P<file>[^:]+):(?P<line>\d+)\s+(?P<expr>.*)$")


class CustomTestRunner(TestRunnerBase):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._pending = []

    def on_testing_line_output(self, line):
        click.echo(line, nl=False)

        detail = DETAIL_RE.match(line.rstrip())
        if detail:
            # Held until the suite summary arrives, so a failure is reported
            # with the assertion that caused it rather than on its own.
            self._pending.append(detail)
            return

        summary = SUMMARY_RE.match(line.strip())
        if not summary:
            return

        failed = summary.group("status") == "FAIL"
        source = None
        message = summary.group("detail").strip()
        if failed and self._pending:
            first = self._pending[0]
            source = TestCaseSource(first.group("file"), int(first.group("line")))
            message = "; ".join(m.group("expr") for m in self._pending)

        self.test_suite.add_case(
            TestCase(
                name=summary.group("name"),
                status=TestStatus.FAILED if failed else TestStatus.PASSED,
                message=message or None,
                source=source,
            )
        )
        self._pending = []
