"""Tests for XADD transformations: substitute, with_order, undefined_to_zero,
prune, get_prunes, and get_support (incl. the trivial-decide short circuit)."""

import ddsolve
from helpers import LT, make_cond, pointwise_equal, term


def make_node(order):
    """The XADD `(x3 - 1 > 0) ? 1 : 0` — then leaf `1`, else leaf `0`."""
    xadd = ddsolve.XADD(order)
    return make_cond(order).ite(xadd.get_one(), xadd.get_zero())


class TestUndefinedToZero:
    def test_replaces_undefined_leaf(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        one = xadd.get_one()
        cond = make_cond(variable_order[0])
        partial = cond.ite(one, xadd.get_undefined())
        total = partial.undefined_to_zero()
        assert pointwise_equal(total, cond.ite(one, xadd.get_zero()))
        assert total.get_else().get_term() == ddsolve.LinearTerm([])
        assert not total.get_else().is_false()

    def test_noop_without_undefined(self, variable_order):
        node = make_node(variable_order[0])
        assert pointwise_equal(node.undefined_to_zero(), node)


class TestPrune:
    def test_noop_on_fully_defined(self, variable_order):
        node = make_node(variable_order[0])
        assert pointwise_equal(node.prune(), node)
        assert pointwise_equal(node.prune_full(), node)

    def test_returns_xadd_with_same_order(self, variable_order):
        part = (make_cond(variable_order[0])
                .ite(ddsolve.XADD(variable_order[0]).get_one(),
                     ddsolve.XADD(variable_order[0]).get_undefined()))
        for pruned in (part.prune(), part.prune_full()):
            assert type(pruned) is ddsolve.XADD
            assert pruned.get_order() is variable_order[0]

    def test_redundant_structure_collapses(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        zero = xadd.get_zero()
        cond = make_cond(variable_order[0])
        assert pointwise_equal(cond.ite(zero, zero), zero)


def node_comparisons(dd):
    """All comparisons of the decision nodes reachable from `dd`.

    Plain DFS without memoization; fine for the tiny diagrams used here.
    """
    comps = []
    stack = [dd]
    while stack:
        node = stack.pop()
        if node.is_leaf():
            continue
        comps.append(node.get_comparison())
        stack.append(node.get_then())
        stack.append(node.get_else())
    return comps


def max_with_trivial_pair(order, sat):
    """max() of two XADDs with a trivially decidable leaf-pair comparison.

    `max`/`min` register leaf-pair comparisons directly in the order
    (bypassing `obtain_repr`, which collapses trivial comparisons), so the
    pair comparison survives as a decision node of the result whenever the
    combined path history is UNSAT.  Here `a` tests `x3 < 5` (then -> `hi`,
    else -> `lo`) and `b` tests `x3 < 1` (then -> `hi`, else -> `x3`); the
    cross path `x3 >= 5` with `x3 < 1` is UNSAT, so both directions of the
    leaf pair decide unsat.
    """
    # The pair-comparison orientation depends on leaf interning order (the
    # later-interned leaf becomes the lhs), so each case fixes its constant
    # leaves explicitly: (first-interned, second-interned, pair-lhs-minus-
    # pair-rhs constant).  Keeping the constants obscure avoids clashing
    # with constants interned by other tests.
    first, second, constant = (205, 202, -3) if sat else (103, 105, 2)
    xadd = ddsolve.XADD(order)
    hi = xadd.get_term(term((0, first)))    # interned first
    lo = xadd.get_term(term((0, second)))   # interned second -> lhs
    expected = ddsolve.LinearComparison(term((0, constant)), LT)
    leafx3 = xadd.get_term(term((3, 1)))
    cond_a = ddsolve.XBDD(order).obtain_repr(
        ddsolve.LinearComparison(term((3, 1), (0, -5)), LT))  # x3 < 5
    cond_b = ddsolve.XBDD(order).obtain_repr(
        ddsolve.LinearComparison(term((3, 1), (0, -1)), LT))  # x3 < 1
    a = cond_a.ite(hi, lo)
    b = cond_b.ite(hi, leafx3)
    m = a.max(b)
    assert expected in order.get_comparisons()
    return m, expected


class TestDecideTrivialCheckShortCircuit:
    """Pin the bypass behavior closest to an unreachable short-circuit.

    Trivially decidable comparisons never become diagram nodes through
    `obtain_repr` (they collapse to constant leaves), but `max`/`min`
    register leaf-pair comparisons directly, so they can survive as
    decision nodes.  These tests pin that behavior, and that `prune_full`
    drops the pair node's unsat-decided paths.
    """

    def test_trivially_unsat_pair_comparison(self, variable_order):
        order = variable_order[0]
        m, trivial = max_with_trivial_pair(order, sat=False)
        # The trivially unsat pair comparison `105 < 103` is a decision
        # node of the max() result and its paths are decided unsat, so
        # prune_full drops them.
        assert trivial in node_comparisons(m)
        assert m.prune_full().get_nodecount() < m.get_nodecount()
        # The other decide-path entries run without raising.
        assert m.prune().get_nodecount() > 0
        assert not m.get_prunes().is_leaf()

    def test_trivially_sat_pair_comparison(self, variable_order):
        order = variable_order[0]
        m, trivial = max_with_trivial_pair(order, sat=True)
        # The trivially sat pair comparison `202 < 205` is a decision
        # node of the max() result; its (unsat-history) paths are still
        # decided unsat by the solver and pruned.
        assert trivial in node_comparisons(m)
        assert m.prune_full().get_nodecount() < m.get_nodecount()
        assert m.prune().get_nodecount() > 0
        assert not m.get_prunes().is_leaf()


class TestSupportAndPrunes:
    def test_support_of_constant(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        support = xadd.get_one().get_support()
        assert type(support) is ddsolve.XBDD
        assert support.is_leaf()
        assert support.is_true()

    def test_support_of_node(self, variable_order):
        node = make_node(variable_order[0])
        support = node.get_support()
        assert type(support) is ddsolve.XBDD
        assert not support.is_leaf()
        assert support.get_order() is variable_order[0]

    def test_prunes_of_constant(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        prunes = xadd.get_one().get_prunes()
        assert type(prunes) is ddsolve.XBDD
        assert prunes.is_leaf()
        assert prunes.is_true()

    def test_prunes_of_node(self, variable_order):
        node = make_node(variable_order[0])
        prunes = node.get_prunes()
        assert type(prunes) is ddsolve.XBDD
        assert prunes.get_order() is variable_order[0]


class TestWithOrder:
    def test_moves_to_new_order(self, variable_order):
        node = make_node(variable_order[0])
        new_order = ddsolve.VariableOrder()
        moved = node.with_order(new_order)
        assert moved.get_order() is new_order
        assert new_order.get_comparisons() == [
            ddsolve.LinearComparison(term((0, 1), (3, -1)),
                                     ddsolve.ComparisonType.kLessThan)
        ]

    def test_top_comparison_preserved(self, variable_order):
        node = make_node(variable_order[0])
        moved = node.with_order(ddsolve.VariableOrder())
        assert moved.get_order() is not variable_order[0]
        assert moved.get_comparison() == node.get_comparison()


class TestSubstitute:
    def test_substitute_to_decided_condition(self, variable_order):
        # Both x3 := 0 and x3 := 1 make `x3 - 1 > 0` false, so the diagram
        # collapses to the else leaf `0`.
        node = make_node(variable_order[0])
        for value in (term(), term((0, 1))):
            sub = node.substitute(3, value)
            assert sub.is_leaf()
            assert sub.get_term() == ddsolve.LinearTerm([])

    def test_substitute_identity(self, variable_order):
        node = make_node(variable_order[0])
        assert pointwise_equal(node.substitute(3, term((3, 1))), node)

    def test_substitute_rewrites_leaf_terms(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        a = xadd.get_term(term((0, 1), (3, -1)))
        sub = a.substitute(3, term((3, 1)))
        expected = xadd.get_term(term((0, 1), (3, -1)))
        # Identity substitution keeps the term.
        assert pointwise_equal(sub, expected)
        # A real rewrite: x3 := 2*x3.
        sub2 = a.substitute(3, term((3, 2)))
        assert pointwise_equal(sub2, xadd.get_term(term((0, 1), (3, -2))))
