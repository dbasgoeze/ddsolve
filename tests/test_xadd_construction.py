"""Tests for XADD construction, constants, navigation, node counts, and to_dot."""

import ddsolve
from helpers import make_cond, term


def make_node(order):
    """The XADD `(x3 - 1 > 0) ? 1 : 0` — then leaf `1`, else leaf `0`."""
    xadd = ddsolve.XADD(order)
    return make_cond(order).ite(xadd.get_one(), xadd.get_zero())


class TestConstruction:
    def test_with_order(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        assert xadd.get_order() is variable_order[0]

    def test_default(self):
        xadd = ddsolve.XADD()
        assert isinstance(xadd.get_order(), ddsolve.VariableOrder)
        # An empty diagram is an undefined leaf.
        assert xadd.is_leaf()
        assert not xadd.is_true()
        assert xadd.is_false()


class TestConstants:
    def test_undefined(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        undef = xadd.get_undefined()
        assert undef.is_leaf()
        assert not undef.is_true()
        assert undef.is_false()
        # The undefined leaf carries no term.
        assert undef.get_term() is None

    def test_zero(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        zero = xadd.get_zero()
        assert zero.is_leaf()
        assert zero.is_true()  # non-zero-leaf check: it is defined
        assert not zero.is_false()
        assert zero.get_term() == ddsolve.LinearTerm([])

    def test_one(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        one = xadd.get_one()
        assert one.is_leaf()
        assert one.get_term() == term((0, 1))

    def test_infty(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        inf = xadd.get_infty()
        assert inf.is_leaf()
        assert inf.get_term() == ddsolve.LinearTerm(
            ddsolve.FinityType.kPositiveInfinite
        )

    def test_neg_infty(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        neg_infty = xadd.get_neg_infty()
        infty = xadd.get_infty()
        assert neg_infty.is_leaf()
        assert neg_infty.get_term() == ddsolve.LinearTerm(
            ddsolve.FinityType.kNegativeInfinite
        )
        # The two infinities are distinct leaves.
        assert neg_infty != infty

    def test_constants_share_order(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        for const in (
            xadd.get_undefined(),
            xadd.get_zero(),
            xadd.get_one(),
            xadd.get_infty(),
        ):
            assert const.get_order() is variable_order[0]

    def test_get_term_factory(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        diag = xadd.get_term(term((3, 1)))
        assert diag.is_leaf()
        assert diag.get_term() == term((3, 1))


class TestNavigation:
    def test_then_else_of_node(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        node = make_node(variable_order[0])
        then_, else_ = node.get_then(), node.get_else()
        assert then_.is_leaf()
        assert then_.get_term() == xadd.get_one().get_term()
        assert else_.is_leaf()
        assert else_.get_term() == xadd.get_zero().get_term()
        assert then_.get_order() is variable_order[0]
        assert else_.get_order() is variable_order[0]

    def test_top_comparison(self, variable_order):
        node = make_node(variable_order[0])
        expected = ddsolve.LinearComparison(term((0, 1), (3, -1)),
                                            ddsolve.ComparisonType.kLessThan)
        assert node.get_comparison() == expected
        assert (node.get_comparison()
                == variable_order[0].get_comparisons()[0])

    def test_leaf_has_no_top_comparison(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        assert xadd.get_one().get_comparison() is None


class TestNodeCount:
    def test_leaf_count(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        assert xadd.get_one().get_nodecount() == 1

    def test_node_count(self, variable_order):
        # One internal node plus two terminal leaves.
        assert make_node(variable_order[0]).get_nodecount() == 3


class TestToDot:
    def test_returns_dot_string(self, variable_order):
        node = make_node(variable_order[0])
        dot = node.to_dot()
        assert isinstance(dot, str)
        assert dot.startswith('digraph "DD"')

    def test_leaf_dot(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        assert xadd.get_one().to_dot().startswith('digraph "DD"')
