"""Tests for XBDD transformations: with_order, substitute, get_support,
and to_dot.  See tests/test_xbdd.py for construction/obtain_repr."""

import ddsolve
from helpers import LT, gt_one, make_cond, term

# A non-leaf XBDD encoding `x3 - 1 > 0`, canonicalized to `1 - x3 < 0`.
make_node = make_cond


def canonical_comp():
    """The canonical `kLessThan` form of `x3 - 1 > 0`."""
    return ddsolve.LinearComparison(term((0, 1), (3, -1)), LT)


class TestWithOrder:
    def test_moves_to_new_order(self, variable_order):
        node = make_node(variable_order[0])
        new_order = ddsolve.VariableOrder()
        moved = node.with_order(new_order)
        assert moved.get_order() is new_order
        # The comparison is copied over to the new order.
        assert new_order.get_comparisons() == [canonical_comp()]

    def test_top_comparison_preserved(self, variable_order):
        node = make_node(variable_order[0])
        moved = node.with_order(ddsolve.VariableOrder())
        assert moved.get_comparison() == canonical_comp()
        assert moved.get_order() is not variable_order[0]


class TestSubstitute:
    def test_substitute_to_constant_false(self, variable_order):
        # x3 := 0 turns `1 - x3 < 0` into `1 < 0`.
        node = make_node(variable_order[0])
        assert node.substitute(3, term()).is_false()

    def test_substitute_to_constant_false_with_one(self, variable_order):
        # x3 := 1 turns `1 - x3 < 0` into `0 < 0`.
        node = make_node(variable_order[0])
        assert node.substitute(3, term((0, 1))).is_false()

    def test_substitute_identity(self, variable_order):
        node = make_node(variable_order[0])
        assert node.substitute(3, term((3, 1))) == node

    def test_substitute_removes_variable(self, variable_order):
        # The constant carrier `x0 := 0` removes the literal, leaving the
        # node over `x3` alone (`-x3 < 0`).
        node = make_node(variable_order[0])
        sub = node.substitute(0, term())
        assert not sub.is_leaf()
        expected = ddsolve.LinearComparison(term((3, -1)), LT)
        assert sub.get_comparison() == expected


class TestSupport:
    def test_support_of_constant(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        support = bdd.get_true().get_support()
        assert support.is_leaf()
        assert support.is_true()

    def test_support_is_xbdd_with_same_order(self, variable_order):
        node = make_node(variable_order[0])
        support = node.get_support()
        assert type(support) is ddsolve.XBDD
        assert support.get_order() is variable_order[0]


class TestToDot:
    def test_returns_dot_string(self, variable_order):
        node = make_node(variable_order[0])
        dot = node.to_dot()
        assert isinstance(dot, str)
        assert dot.startswith('digraph "DD"')

    def test_leaf_dot(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        assert bdd.get_false().to_dot().startswith('digraph "DD"')
