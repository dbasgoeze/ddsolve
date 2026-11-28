"""Tests for SmtTerm bindings: functions, arrays, and type casts."""

import pytest

import ddsolve


class TestSmtTermFunctionCreation:
    """Test SmtTerm.mk_int_func and mk_real_func: arity and typing."""

    def test_mk_int_func(self):
        f = ddsolve.SmtTerm.mk_int_func("f", 2)
        assert repr(f) == "f"

    def test_mk_int_func_arity_1(self):
        g = ddsolve.SmtTerm.mk_int_func("g", 1)
        assert repr(g) == "g"
        # A 1-arity Int function applies to exactly one Int argument.
        applied = g.apply_func([ddsolve.SmtTerm.mk_int_var("g_x")])
        assert repr(applied) == "(g g_x)"

    def test_mk_real_func(self):
        h = ddsolve.SmtTerm.mk_real_func("h", 3)
        assert repr(h) == "h"
        # A Real function applies to Real arguments of the declared arity.
        applied = h.apply_func([
            ddsolve.SmtTerm.mk_real_var("h_r1"),
            ddsolve.SmtTerm.mk_real_var("h_r2"),
            ddsolve.SmtTerm.mk_real("2.5"),
        ])
        assert repr(applied) == "(h h_r1 h_r2 (/ 5 2))"

    def test_apply_func_wrong_arity_raises(self):
        """Applying with more arguments than the declared arity raises."""
        g = ddsolve.SmtTerm.mk_int_func("g_wrong_arity", 1)
        with pytest.raises(RuntimeError):
            g.apply_func([ddsolve.SmtTerm.mk_int_var("g_x"),
                          ddsolve.SmtTerm.mk_int("5")])

    def test_apply_func_type_mismatch_raises(self):
        """An Int function cannot be applied to a Real argument."""
        fi = ddsolve.SmtTerm.mk_int_func("fi", 1)
        with pytest.raises(RuntimeError):
            fi.apply_func([ddsolve.SmtTerm.mk_real_var("fi_r")])


class TestSmtTermArrayCreation:
    """Test SmtTerm.mk_int_array and mk_real_array."""

    def test_mk_int_array(self):
        assert repr(ddsolve.SmtTerm.mk_int_array("myarr")) == "myarr"

    def test_mk_real_array(self):
        arr = ddsolve.SmtTerm.mk_real_array("real_arr")
        assert repr(arr) == "real_arr"
        # A Real array stores and selects Real values (the Int-array
        # store/select chains are covered in TestSmtTermArrays).
        idx = ddsolve.SmtTerm.mk_int("0")
        stored = arr.array_store(idx, ddsolve.SmtTerm.mk_real("2.5"))
        assert repr(stored) == "(store real_arr 0 (/ 5 2))"
        # Selecting through the store constant-folds to the stored value.
        assert repr(stored.array_select(idx)) == "(/ 5 2)"


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
