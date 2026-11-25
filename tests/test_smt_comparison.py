"""Tests for SmtComparison: push/pop_assumption, quantification,
comparison operators, substitute, and __repr__."""

import subprocess
import sys

import ddsolve


class TestSmtComparisonPushPopAssumption:
    """Test push/pop_assumption stack mechanics."""

    def test_push_and_pop_single(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("5"),
                                     ddsolve.ComparisonType.kLessThan)
        wp = ddsolve.SmtComparison.WithParity(True, comp)
        ddsolve.SmtComparison.push_assumption(wp)
        ddsolve.SmtComparison.pop_assumption()

    def test_push_and_pop_multiple(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        y = ddsolve.SmtTerm.mk_int_var("y")
        comp1 = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("5"),
                                      ddsolve.ComparisonType.kLessThan)
        comp2 = ddsolve.SmtComparison(y, ddsolve.SmtTerm.mk_int("3"),
                                      ddsolve.ComparisonType.kGreaterThan)
        wp1 = ddsolve.SmtComparison.WithParity(True, comp1)
        wp2 = ddsolve.SmtComparison.WithParity(False, comp2)
        ddsolve.SmtComparison.push_assumption(wp1)
        ddsolve.SmtComparison.push_assumption(wp2)
        ddsolve.SmtComparison.pop_assumption()
        ddsolve.SmtComparison.pop_assumption()

    def test_push_negative_parity(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("0"),
                                     ddsolve.ComparisonType.kGreaterThan)
        wp = ddsolve.SmtComparison.WithParity(False, comp)
        ddsolve.SmtComparison.push_assumption(wp)
        ddsolve.SmtComparison.pop_assumption()

    def test_pop_without_push_crashes(self):
        """Popping without a matching push causes a segfault/abort.

        This is documented as current behavior — the test runs in a
        subprocess so a crash does not poison the rest of the suite.
        """
        result = subprocess.run(
            [sys.executable, "-c",
             "import ddsolve; ddsolve.SmtComparison.pop_assumption()"],
            capture_output=True, timeout=30,
        )
        # The process crashes (non-zero exit) due to the mismatch
        assert result.returncode != 0


class TestSmtComparisonMkForallInts:
    """Test mk_forall_ints — universal quantification over int variables.

    The simplifier evaluates the quantified formula: universally true
    constraints reduce to the leaf `"true"`, refutable ones to `"false"`.
    """

    def test_simple_forall(self):
        # forall x. 2x < 10 is refuted by large x.
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x + x, ddsolve.SmtTerm.mk_int("10"),
                                     ddsolve.ComparisonType.kLessThan)
        forall = ddsolve.SmtComparison.mk_forall_ints(x, comp)
        assert repr(forall) == "false"

    def test_true_forall(self):
        # forall x. x < x + 1 holds everywhere.
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x, x + ddsolve.SmtTerm.mk_int("1"),
                                     ddsolve.ComparisonType.kLessThan)
        forall = ddsolve.SmtComparison.mk_forall_ints(x, comp)
        assert repr(forall) == "true"

    def test_forall_with_negation(self):
        # forall x. ~(x < 100) is refuted by x = 0.
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("100"),
                                     ddsolve.ComparisonType.kLessThan)
        neg = ~comp
        forall = ddsolve.SmtComparison.mk_forall_ints(x, neg)
        assert repr(forall) == "false"

    def test_forall_with_arithmetic_term(self):
        # forall x. x + 1 < 10 is refuted by large x.
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x + ddsolve.SmtTerm.mk_int("1"),
                                     ddsolve.SmtTerm.mk_int("10"),
                                     ddsolve.ComparisonType.kLessThan)
        forall = ddsolve.SmtComparison.mk_forall_ints(x, comp)
        assert repr(forall) == "false"

    def test_forall_multiple_vars(self):
        # forall x. x + y <= 0 is refuted by large x; a second quantifier
        # over y of the already-refuted formula stays refuted.
        x = ddsolve.SmtTerm.mk_int_var("x")
        y = ddsolve.SmtTerm.mk_int_var("y")
        comp = ddsolve.SmtComparison(x + y, ddsolve.SmtTerm.mk_int("0"),
                                     ddsolve.ComparisonType.kLessThanOrEqual)
        forall_x = ddsolve.SmtComparison.mk_forall_ints(x, comp)
        assert repr(forall_x) == "false"
        forall_y = ddsolve.SmtComparison.mk_forall_ints(y, forall_x)
        assert repr(forall_y) == "false"


class TestSmtComparisonMkForallReals:
    """Test mk_forall_reals — universal quantification over real variables."""

    def test_simple_forall_real(self):
        # forall x. 2x < 1 is refuted by large x.
        x = ddsolve.SmtTerm.mk_real_var("x")
        comp = ddsolve.SmtComparison(x * ddsolve.SmtTerm.mk_real("2"),
                                     ddsolve.SmtTerm.mk_real("1"),
                                     ddsolve.ComparisonType.kLessThan)
        forall = ddsolve.SmtComparison.mk_forall_reals(x, comp)
        assert repr(forall) == "false"

    def test_true_forall_real(self):
        # forall x. x < x + 1 holds everywhere.
        x = ddsolve.SmtTerm.mk_real_var("x")
        comp = ddsolve.SmtComparison(x, x + ddsolve.SmtTerm.mk_real("1"),
                                     ddsolve.ComparisonType.kLessThan)
        forall = ddsolve.SmtComparison.mk_forall_reals(x, comp)
        assert repr(forall) == "true"

    def test_forall_real_with_arithmetic(self):
        # forall x. 2x < 10 is refuted by large x.
        x = ddsolve.SmtTerm.mk_real_var("x")
        comp = ddsolve.SmtComparison(x + x, ddsolve.SmtTerm.mk_real("10"),
                                     ddsolve.ComparisonType.kLessThan)
        forall = ddsolve.SmtComparison.mk_forall_reals(x, comp)
        assert repr(forall) == "false"

    def test_forall_real_identity(self):
        # forall x. x >= 0 is refuted by negative x.
        x = ddsolve.SmtTerm.mk_real_var("x")
        comp = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_real("0"),
                                     ddsolve.ComparisonType.kGreaterThanOrEqual)
        forall = ddsolve.SmtComparison.mk_forall_reals(x, comp)
        assert repr(forall) == "false"


class TestSmtComparisonLtEq:
    """Test __lt__ and __eq__ on SmtComparison."""

    def test_lt(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        c1 = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("5"),
                                   ddsolve.ComparisonType.kLessThan)
        c2 = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("10"),
                                   ddsolve.ComparisonType.kLessThan)
        # `a < b` is a strict refinement order: `x < 5` is strictly
        # stronger than `x < 10`, so `c2 < c1` holds, but not the converse,
        # and a comparison never orders below itself.
        assert c2 < c1
        assert not (c1 < c2)
        assert not (c1 < c1)

    def test_eq(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        c1 = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("5"),
                                   ddsolve.ComparisonType.kLessThan)
        c2 = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("5"),
                                   ddsolve.ComparisonType.kLessThan)
        assert c1 == c2

    def test_neq(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        y = ddsolve.SmtTerm.mk_int_var("y")
        c1 = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("5"),
                                   ddsolve.ComparisonType.kLessThan)
        c2 = ddsolve.SmtComparison(y, ddsolve.SmtTerm.mk_int("5"),
                                   ddsolve.ComparisonType.kLessThan)
        assert c1 != c2


class TestSmtComparisonSubstitute:
    """Test SmtComparison.substitute()."""

    def test_substitute_to_decided_comparison(self):
        # Substituting x := 3 decides the comparison: 3 < 5 is true.
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("5"),
                                     ddsolve.ComparisonType.kLessThan)
        substituted = comp.substitute(x, ddsolve.SmtTerm.mk_int("3"))
        assert repr(substituted) == "true"

    def test_substitute_identity_keeps_comparison(self):
        # Substituting x := x leaves the comparison unchanged.
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("5"),
                                     ddsolve.ComparisonType.kLessThan)
        assert repr(comp.substitute(x, x)) == "(not (>= x 5))"

    def test_substitute_arithmetic_term(self):
        # Substituting x := 2 into 2x < 10 decides it: 4 < 10 is true.
        x = ddsolve.SmtTerm.mk_int_var("x")
        two_x = x + x
        comp = ddsolve.SmtComparison(two_x, ddsolve.SmtTerm.mk_int("10"),
                                     ddsolve.ComparisonType.kLessThan)
        substituted = comp.substitute(x, ddsolve.SmtTerm.mk_int("2"))
        assert repr(substituted) == "true"


class TestSmtComparisonRepr:
    """Test SmtComparison __repr__ (SMT-LIB style, simplified)."""

    def test_less_than_repr(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("5"),
                                     ddsolve.ComparisonType.kLessThan)
        # Canonicalized to the negation of `x >= 5`.
        assert repr(comp) == "(not (>= x 5))"

    def test_equality_repr(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("0"),
                                     ddsolve.ComparisonType.kEquality)
        assert repr(comp) == "(= x 0)"

    def test_arithmetic_repr(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x + x, ddsolve.SmtTerm.mk_int("10"),
                                     ddsolve.ComparisonType.kLessThan)
        # 2x < 10 simplifies to x < 5.
        assert repr(comp) == "(not (>= x 5))"

    def test_and_repr(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        c1 = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("0"),
                                   ddsolve.ComparisonType.kGreaterThanOrEqual)
        c2 = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("10"),
                                   ddsolve.ComparisonType.kLessThan)
        combined = c1 & c2
        assert repr(combined) == "(and (>= x 0) (not (>= x 10)))"

    def test_or_repr(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        c1 = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("0"),
                                   ddsolve.ComparisonType.kGreaterThanOrEqual)
        c2 = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("10"),
                                   ddsolve.ComparisonType.kLessThan)
        combined = c1 | c2
        assert repr(combined) == "(or (>= x 0) (not (>= x 10)))"

    def test_not_repr(self):
        x = ddsolve.SmtTerm.mk_int_var("x")
        comp = ddsolve.SmtComparison(x, ddsolve.SmtTerm.mk_int("5"),
                                     ddsolve.ComparisonType.kLessThan)
        # The negation of `x < 5` is canonicalized to `x >= 5`.
        assert repr(~comp) == "(>= x 5)"
