"""Tests for XADD arithmetic: +, -, max, min, and scale."""

import pytest

import ddsolve
from helpers import LT, make_cond, pointwise_equal, term


def make_node(order):
    """The XADD `(x3 - 1 > 0) ? 1 : 0` — then leaf `1`, else leaf `0`."""
    xadd = ddsolve.XADD(order)
    return make_cond(order).ite(xadd.get_one(), xadd.get_zero())


class TestArithmetic:
    def test_add_constants(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        one = xadd.get_one()
        two = xadd.get_term(term((0, 2)))
        assert pointwise_equal(one + one, two)
        assert pointwise_equal(two - one, one)

    def test_add_node(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        node = make_node(variable_order[0])
        shifted = node + xadd.get_one()
        assert shifted.get_then().get_term() == term((0, 2))
        assert shifted.get_else().get_term() == term((0, 1))

    def test_add_leaf_terms(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        a = xadd.get_term(term((0, 1), (3, -1)))   # 1 - x3
        b = xadd.get_term(term((3, 1)))
        # (1 - x3) + x3 = 1
        assert pointwise_equal(a + b, xadd.get_one())

    def test_sub_node(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        node = make_node(variable_order[0])
        shifted = node - xadd.get_one()
        # 1 - 1 = 0: a defined zero-value leaf (not the undefined one).
        assert shifted.get_then().get_term() == ddsolve.LinearTerm([])
        assert shifted.get_then().is_true()
        # 0 - 1 = -1.
        assert shifted.get_else().get_term() == term((0, -1))

    def test_mixed_orders_raise(self):
        a = make_node(ddsolve.VariableOrder())
        b = ddsolve.XADD(ddsolve.VariableOrder()).get_one()
        for op in ("add", "sub"):
            with pytest.raises(ValueError):
                if op == "add":
                    a + b
                else:
                    a - b


class TestMinMax:
    def test_max_constants(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        one, two = xadd.get_one(), xadd.get_term(term((0, 2)))
        assert pointwise_equal(one.max(two), two)
        assert pointwise_equal(two.min(one), one)

    def test_max_min_pointwise(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        one, zero = xadd.get_one(), xadd.get_zero()
        cond = make_cond(variable_order[0])
        f = cond.ite(one, zero)
        g = cond.ite(xadd.get_term(term((0, 3))), zero)
        assert f.max(g).get_then().get_term() == term((0, 3))
        assert f.max(g).get_else().get_term() == ddsolve.LinearTerm([])
        assert f.min(g).get_then().get_term() == term((0, 1))
        assert f.min(g).get_else().get_term() == ddsolve.LinearTerm([])

    def test_mixed_orders_raise(self):
        a = make_node(ddsolve.VariableOrder())
        b = ddsolve.XADD(ddsolve.VariableOrder()).get_one()
        with pytest.raises(ValueError):
            a.max(b)
        with pytest.raises(ValueError):
            a.min(b)

    def test_min_max_of_crossing_leaf_terms_case_splits(self, variable_order):
        """min/max of two leaf diagrams with distinct linear terms.

        Both must case-split on the crossing condition (Algorithm 4,
        two-terminal case) instead of collapsing to a single leaf, which
        would silently discard one operand.  The implementation splits on
        the *flipped* comparison (`x3 < 20/3`, the `not (A < B)` form)
        with swapped branches — semantically identical, only the canonical
        form differs from the algorithm's wording.
        """
        xadd = ddsolve.XADD(variable_order[0])
        a = xadd.get_term(term((0, 1), (3, "-1/10")))   # 1 - x3/10
        b = xadd.get_term(term((3, "1/20")))            # x3/20
        # They cross at x3 = 20/3.
        minimum = a.min(b)
        maximum = a.max(b)
        assert not minimum.is_leaf()
        assert not maximum.is_leaf()
        cond_flip = ddsolve.XBDD(variable_order[0]).obtain_repr(
            ddsolve.LinearComparison(term((0, "-1"), (3, "3/20")),
                                     ddsolve.ComparisonType.kLessThan))
        # Pointwise equality, plus the branch layout.
        assert pointwise_equal(minimum, cond_flip.ite(b, a))
        assert pointwise_equal(maximum, cond_flip.ite(a, b))
        assert minimum.get_then().get_term() == b.get_term()
        assert minimum.get_else().get_term() == a.get_term()
        assert maximum.get_then().get_term() == a.get_term()
        assert maximum.get_else().get_term() == b.get_term()


class TestScale:
    def test_scale_node(self, variable_order):
        node = make_node(variable_order[0])
        scaled = node.scale("2")
        assert scaled.get_then().get_term() == term((0, 2))
        assert scaled.get_else().get_term() == ddsolve.LinearTerm([])

    def test_scale_half(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        two = xadd.get_term(term((0, 2)))
        assert pointwise_equal(two.scale("1/2"), xadd.get_one())

    def test_scale_leaf_term(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        a = xadd.get_term(term((0, 1), (3, -1)))
        scaled = a.scale("2")
        assert pointwise_equal(scaled,
                               xadd.get_term(term((0, 2), (3, -2))))

    def test_invalid_scalar_raises(self, variable_order):
        node = make_node(variable_order[0])
        with pytest.raises(ValueError):
            node.scale("not-a-number")
