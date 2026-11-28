"""Tests for XADD comparison operators: ==, <=, < (pointwise semantics)."""

import pytest

import ddsolve
from helpers import make_cond, pointwise_equal, term


def make_node(order):
    """The XADD `(x3 - 1 > 0) ? 1 : 0` — then leaf `1`, else leaf `0`."""
    xadd = ddsolve.XADD(order)
    return make_cond(order).ite(xadd.get_one(), xadd.get_zero())


class TestComparisonOps:
    def test_constant_comparisons(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        one, two = xadd.get_one(), xadd.get_term(term((0, 2)))
        assert one <= two
        assert not (two <= one)
        assert one < two
        assert not (two < one)
        assert not (one < one)

    def test_comparison_is_universal_pointwise(self, variable_order):
        """`<` / `<=` hold iff the relation holds at *every* point.

        Crossing leaf terms are the discriminating case: they satisfy the
        relation on some of the domain only.
        """
        xadd = ddsolve.XADD(variable_order[0])
        # A = 1 - x3, B = x3/20, B+1 = 1 + x3/20; all over x3 >= 0.
        a = xadd.get_term(term((0, 1), (3, -1)))
        b = xadd.get_term(term((3, 1)))
        b_plus_one = xadd.get_term(term((0, 1), (3, 1)))
        # A <= B fails at x3 = 0 (1 <= 0 is false).
        assert not (a <= b)
        assert not (a < b)
        # A <= B+1 holds everywhere (-x3 <= x3/20 for x3 >= 0), but A < B+1
        # fails at x3 = 0 where they are equal.
        assert a <= b_plus_one
        assert not (a < b_plus_one)
        # B < A+1 fails for large x3 (needs 3*x3/20 < 2, i.e. x3 < 40/3).
        assert not (b < (a + xadd.get_one()))

    def test_pointwise_comparisons(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        one, zero = xadd.get_one(), xadd.get_zero()
        cond = make_cond(variable_order[0])
        f = cond.ite(one, zero)
        g = cond.ite(xadd.get_term(term((0, 3))), zero)
        # Pointwise: 1 <= 3 and 0 <= 0 hold; strict < fails at the 0 leaf.
        assert f <= g
        assert not (f < g)

    def test_pointwise_equality(self, variable_order):
        """Pointwise equality: `f == g` iff `f <= g` and `f >= g`."""
        xadd = ddsolve.XADD(variable_order[0])
        one, zero = xadd.get_one(), xadd.get_zero()
        cond = make_cond(variable_order[0])
        f = cond.ite(one, zero)
        # A diagram is pointwise equal to itself and to a rebuild.
        assert pointwise_equal(f, f)
        assert pointwise_equal(f, cond.ite(one, zero))
        # A different function on the same structure is not equal.
        g = cond.ite(xadd.get_term(term((0, 2))), zero)
        assert not pointwise_equal(f, g)
        # Crossing leaf terms: neither `a <= b` nor `b <= a` holds.
        a = xadd.get_term(term((0, 1), (3, -1)))
        b = xadd.get_term(term((3, 1)))
        assert not pointwise_equal(a, b)

    def test_mixed_orders_raise(self, variable_order):
        a = make_node(variable_order[0])
        b = ddsolve.XADD(ddsolve.VariableOrder()).get_one()
        for op in ("lt", "le"):
            with pytest.raises(ValueError):
                if op == "lt":
                    a < b
                else:
                    a <= b

    def test_pointwise_equality_across_canonical_forms(self, variable_order):
        """Pointwise equality aligns diagrams with different case splits.

        `min(1 - x3, x3)` splits on `-1 + 2*x3 < 0`; the same function can
        be written with the equivalent threshold `-1/2 + x3 < 0`, which
        becomes a *different* sylvan variable.  The histories threaded
        through the comparison align the two case splits, so the diagrams
        compare pointwise-equal despite being syntactically distinct.
        """
        xadd = ddsolve.XADD(variable_order[0])
        a = xadd.get_term(term((0, 1), (3, -1)))   # 1 - x3
        b = xadd.get_term(term((3, 1)))            # x3
        f = a.min(b)                               # split on -1 + 2*x3 < 0
        cond = ddsolve.XBDD(variable_order[0]).obtain_repr(
            ddsolve.LinearComparison(term((3, 1), (0, "-1/2")),
                                     ddsolve.ComparisonType.kLessThan))
        g = cond.ite(b, a)                         # split on -1/2 + x3 < 0
        assert f != g
        assert pointwise_equal(f, g)

        # Negative control: threshold `x3 < 2` differs at x3 = 1, where
        # min(1 - x3, x3) = 0 but `x3` gives 1 (the failure witness).
        cond2 = ddsolve.XBDD(variable_order[0]).obtain_repr(
            ddsolve.LinearComparison(term((3, 1), (0, "-2")),
                                     ddsolve.ComparisonType.kLessThan))
        h = cond2.ite(b, a)
        assert not pointwise_equal(h, f)
        assert not pointwise_equal(f, h)

    def test_equality(self, variable_order):
        node = make_node(variable_order[0])
        assert node == node
        assert make_node(variable_order[0]) == node
        assert node != make_node(ddsolve.VariableOrder())  # order identity
