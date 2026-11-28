"""Tests for SmtComparison quantification: mk_forall_ints and
mk_forall_reals.  These invoke the cvc5 solver (quantifier elimination)
and are comparatively slow."""

import ddsolve


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
