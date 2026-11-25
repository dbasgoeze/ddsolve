"""Tests for SmtTerm bindings: functions, arrays, type casts, `*` and `/`.

The `*` and `/` operations leave unreleased Sylvan refs, so their tests
run in subprocesses to avoid poisoning the main test process.
"""

import subprocess
import sys

import pytest

import ddsolve


class TestSmtTermFunctionCreation:
    """Test SmtTerm.mk_int_func and mk_real_func."""

    def test_mk_int_func(self):
        f = ddsolve.SmtTerm.mk_int_func("f", 2)
        assert repr(f) == "f"

    def test_mk_int_func_arity_1(self):
        assert repr(ddsolve.SmtTerm.mk_int_func("g", 1)) == "g"

    def test_mk_real_func(self):
        assert repr(ddsolve.SmtTerm.mk_real_func("h", 3)) == "h"


class TestSmtTermArrayCreation:
    """Test SmtTerm.mk_int_array and mk_real_array."""

    def test_mk_int_array(self):
        assert repr(ddsolve.SmtTerm.mk_int_array("myarr")) == "myarr"

    def test_mk_real_array(self):
        assert repr(ddsolve.SmtTerm.mk_real_array("real_arr")) == "real_arr"


class TestSmtTermApplyFunction:
    """Test SmtTerm.apply_func."""

    def test_apply_func(self):
        f = ddsolve.SmtTerm.mk_int_func("f", 2)
        applied = f.apply_func([ddsolve.SmtTerm.mk_int_var("x"),
                                ddsolve.SmtTerm.mk_int("5")])
        assert repr(applied) == "(f x 5)"

    def test_apply_func_nested(self):
        """Apply a function to a function application."""
        # SMT symbol names are global to the process; use unique names so
        # the declaration in test_apply_func (arity 2, "f") doesn't collide.
        f1 = ddsolve.SmtTerm.mk_int_func("f1", 1)
        g1 = ddsolve.SmtTerm.mk_int_func("g1", 1)
        gx = g1.apply_func([ddsolve.SmtTerm.mk_int_var("x")])
        assert repr(gx) == "(g1 x)"
        assert repr(f1.apply_func([gx])) == "(f1 (g1 x))"

    def test_apply_func_zero_args_raises(self):
        """Zero-arity function application raises RuntimeError."""
        f = ddsolve.SmtTerm.mk_int_func("zero", 0)
        with pytest.raises(RuntimeError):
            f.apply_func([])


class TestSmtTermArrays:
    """Test SmtTerm.array_store and array_select."""

    def test_array_store_and_select(self):
        arr = ddsolve.SmtTerm.mk_int_array("a")
        idx = ddsolve.SmtTerm.mk_int("0")
        stored = arr.array_store(idx, ddsolve.SmtTerm.mk_int("42"))
        assert repr(stored) == "(store a 0 42)"
        # Selecting through the store constant-folds to the stored value.
        assert repr(stored.array_select(idx)) == "42"

    def test_array_select_on_original(self):
        arr = ddsolve.SmtTerm.mk_int_array("a")
        idx = ddsolve.SmtTerm.mk_int("0")
        assert repr(arr.array_select(idx)) == "(select a 0)"

    def test_array_store_nested(self):
        arr = ddsolve.SmtTerm.mk_int_array("a")
        idx = ddsolve.SmtTerm.mk_int("1")
        val = arr.array_select(ddsolve.SmtTerm.mk_int("0"))
        stored = arr.array_store(idx, val)
        assert repr(stored) == "(store a 1 (select a 0))"


class TestSmtTermTypeCasts:
    """Test SmtTerm.to_real and to_int."""

    def test_to_real(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        assert repr(x.to_real()) == "(to_real x)"

    def test_to_real_from_expression(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        assert repr((x + x).to_real()) == "(to_real (* 2 x))"

    def test_to_int(self):
        # Unique variable name: "x" is declared as an Int variable elsewhere,
        # and redeclaring it as a Real would collapse the cast.
        x = ddsolve.SmtTerm.mk_real_var("r")
        assert repr(x.to_int()) == "(to_int r)"

    def test_to_int_from_expression(self):
        x = ddsolve.SmtTerm.mk_real_var("r")
        assert repr((x * ddsolve.SmtTerm.mk_real("2")).to_int()) == (
            "(to_int (* 2.0 r))"
        )


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
