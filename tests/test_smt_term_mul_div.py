"""Tests for SmtTerm `*` and `/`.

These operations leave unreleased Sylvan refs, so their tests run in
subprocesses to avoid poisoning the main test process.
"""

import subprocess
import sys


class TestSmtTermMulDiv:
    """SmtTerm `*` and `/`, run in subprocesses (they leak Sylvan refs)."""

    _CODE = """
import ddsolve
import sys

x = ddsolve.SmtTerm.mk_int_var("x")
y = ddsolve.SmtTerm.mk_int_var("y")

action = sys.argv[1]

if action == "mul":
    print(repr(x * ddsolve.SmtTerm.mk_int("2")))
elif action == "mul_chained":
    print(repr(x * ddsolve.SmtTerm.mk_int("3") * ddsolve.SmtTerm.mk_int("2")))
elif action == "div":
    print(repr(x / ddsolve.SmtTerm.mk_int("2")))
elif action == "div_by_one":
    print(repr(x / ddsolve.SmtTerm.mk_int("1")))
elif action == "substitute_mul":
    prod = x * y
    print(repr(prod.substitute(x, ddsolve.SmtTerm.mk_int("2"))))
elif action == "div_zero":
    # No exception is raised; division by the zero term yields a term.
    print(repr(x / ddsolve.SmtTerm.mk_int("0")))
"""

    def _run(self, action):
        result = subprocess.run(
            [sys.executable, "-c", self._CODE, action],
            capture_output=True, text=True, timeout=60,
        )
        assert result.returncode == 0, (
            f"subprocess failed with {result.returncode}:\n{result.stderr}"
        )
        # Module import may print a graphviz warning to stdout; drop it.
        lines = [line for line in result.stdout.splitlines()
                 if "Missing Dependency" not in line]
        return "\n".join(lines).strip()

    def test_smt_mul(self):
        assert self._run("mul") == "(* 2 x)"

    def test_smt_mul_chained(self):
        assert self._run("mul_chained") == "(* 6 x)"

    def test_smt_div(self):
        # Integer division lifts the term to the reals.
        assert self._run("div") == "(* (/ 1 2) (to_real x))"

    def test_smt_div_by_one(self):
        assert self._run("div_by_one") == "(to_real x)"

    def test_smt_term_substitute_and_mul(self):
        assert self._run("substitute_mul") == "(* 2 y)"

    def test_smt_div_zero(self):
        assert self._run("div_zero") == "(/ (to_real x) 0.0)"
