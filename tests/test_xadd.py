"""Tests for the Python-exposed methods of `ddsolve.XADD`.

Covers every method/operator of the `XADD` class: construction, `to_dot`,
the constant factories (`get_undefined`, `get_zero`, `get_one`, `get_infty`,
`get_neg_infty`), `get_term` (both overloads), `get_then`, `get_else`,
`get_comparison`, `is_leaf`, `is_true`, `is_false`, `get_nodecount`,
`with_order`, `get_order`, `substitute`, `prune`, `prune_full`, `max`,
`min`, `get_support`, `get_prunes`, `undefined_to_zero`, `scale`, the
comparison operators `==`, `<=`, `<`, the arithmetic operators `+` and
`-`, and `XBDD * XADD` (selector product, covered in detail in
`tests/test_xbdd.py`).

Domain notes (mirroring `tests/test_xbdd.py`):
- `Product(0, c)` is the literal constant `c`; `Product(v, c)` with `v > 0`
  is the term `c * x_v`.  Variables are non-negative integers.
- Comparisons are canonicalized to `kLessThan`; the canonical form of
  `x3 - 1 > 0` is `1 + -1*x3 < 0`.

Known quirks of the C++ layer (asserted as current behavior, not
specification):
- `XADD.__eq__` compares variable-order *identity* plus the underlying
  MTBDD, like `XBDD.__eq__`; diagram-level semantic equality is asserted
  via `pointwise_equal` (`f <= g` and `f >= g`), which aligns differing
  case splits via the comparison histories.  Note the SMT backend decides
  over non-negative integers, so thresholds like `x3 < 1` and `x3 < 1/2`
  are interchangeable.
- `prune` / `prune_full` and `get_prunes` semantics on diagrams with
  undefined leaves are murky, so those are only checked lightly.
- For Smt diagrams: `undefined_to_zero` inserts the *default-constructed*
  `SmtTerm()` (an empty term), not an arithmetic zero; `prune_full` may
  restructure even fully-defined diagrams, so only structural properties
  are asserted for it; trivially decided comparisons stay decision nodes
  over a canonical `"true"` comparison.

SmtXADD is covered by the `TestSmt*` classes at the end of this file.
"""

import subprocess
import sys

import pytest

import ddsolve
from helpers import (LT, make_cond, make_smt_xadd, pointwise_equal,
                     smt_cond, term)


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


class TestNodeCount:
    def test_leaf_count(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        assert xadd.get_one().get_nodecount() == 1

    def test_node_count(self, variable_order):
        # One internal node plus two terminal leaves.
        assert make_node(variable_order[0]).get_nodecount() == 3


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


class TestIte:
    def test_ite_with_term_leaves(self, variable_order):
        """ite over leaf diagrams carrying distinct linear terms."""
        xadd = ddsolve.XADD(variable_order[0])
        cond = make_cond(variable_order[0])
        a = xadd.get_term(term((0, 1), (3, -1)))
        b = xadd.get_term(term((3, 1)))
        joined = cond.ite(a, b)
        assert not joined.is_leaf()
        assert joined.get_comparison() == cond.get_comparison()
        assert joined.get_then().get_term() == a.get_term()
        assert joined.get_else().get_term() == b.get_term()

    def test_mixed_orders_raise(self):
        """XBDD::ite(XADD, XADD) throws on order mismatch."""
        order1 = ddsolve.VariableOrder()
        order2 = ddsolve.VariableOrder()
        cond = ddsolve.XBDD(order1).obtain_repr(
            ddsolve.LinearComparison(term((0, 1), (3, -1)),
                                     ddsolve.ComparisonType.kLessThan)
        )
        then_ = ddsolve.XADD(order1).get_one()
        else_ = ddsolve.XADD(order1).get_zero()
        # XADD from a *different* order
        wrong_then = ddsolve.XADD(order2).get_one()
        with pytest.raises(ValueError):
            cond.ite(wrong_then, else_)
        with pytest.raises(ValueError):
            cond.ite(then_, wrong_then)


class TestToDot:
    def test_returns_dot_string(self, variable_order):
        node = make_node(variable_order[0])
        dot = node.to_dot()
        assert isinstance(dot, str)
        assert dot.startswith('digraph "DD"')

    def test_leaf_dot(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        assert xadd.get_one().to_dot().startswith('digraph "DD"')


# ---------------------------------------------------------------------------
# SmtXADD — same interface, SmtTerm leaves and comparisons.
# ---------------------------------------------------------------------------


class TestSmtConstruction:
    def test_with_order(self):
        order = ddsolve.SmtVariableOrder()
        xadd = ddsolve.SmtXADD(order)
        assert xadd.get_order() is order

    def test_default(self):
        xadd = ddsolve.SmtXADD()
        assert isinstance(xadd.get_order(), ddsolve.SmtVariableOrder)
        assert xadd.is_leaf()
        assert not xadd.is_true()
        assert xadd.is_false()


class TestSmtUndefined:
    def test_undefined_leaf(self):
        xadd = ddsolve.SmtXADD(ddsolve.SmtVariableOrder())
        undef = xadd.get_undefined()
        assert undef.is_leaf()
        assert not undef.is_true()
        assert undef.is_false()
        assert undef.get_term() is None


class TestSmtTermLeaves:
    def test_get_term_factory(self):
        order = ddsolve.SmtVariableOrder()
        xadd = ddsolve.SmtXADD(order)
        x = ddsolve.SmtTerm.mk_int_var("x")
        leaf = xadd.get_term(x)
        assert leaf.is_leaf()
        assert leaf.get_term() == x

    def test_arithmetic_on_leaf_terms(self):
        order = ddsolve.SmtVariableOrder()
        xadd = ddsolve.SmtXADD(order)
        x = ddsolve.SmtTerm.mk_int_var("x")
        a = xadd.get_term(x)
        b = xadd.get_term(x + x)
        assert (a + b).get_term() == ddsolve.SmtTerm.mk_int("3") * x
        assert (a - b).get_term() == ddsolve.SmtTerm.mk_int("-1") * x

    def test_multiplication(self):
        """SmtXADD * SmtXADD — basic multiplication."""
        order = ddsolve.SmtVariableOrder()
        xadd = ddsolve.SmtXADD(order)
        x = ddsolve.SmtTerm.mk_int_var("x")
        a = xadd.get_term(x)
        b = xadd.get_term(x + x)
        product = a * b
        assert type(product) is ddsolve.SmtXADD

    def test_mixed_orders_raise(self):
        """SmtXADD::operator* throws on order mismatch."""
        order1 = ddsolve.SmtVariableOrder()
        order2 = ddsolve.SmtVariableOrder()
        xadd1 = ddsolve.SmtXADD(order1)
        xadd2 = ddsolve.SmtXADD(order2)
        x = ddsolve.SmtTerm.mk_int_var("x")
        a = xadd1.get_term(x)
        b = xadd2.get_term(x)
        with pytest.raises(ValueError):
            a * b

    def test_arithmetic_on_node(self):
        node = make_smt_xadd(ddsolve.SmtVariableOrder())
        shifted = node + ddsolve.SmtXADD(node.get_order()).get_term(
            ddsolve.SmtTerm.mk_int("1")
        )
        assert shifted.get_then().get_term() == (
            ddsolve.SmtTerm.mk_int("1") + ddsolve.SmtTerm.mk_int_var("x")
        )
        assert shifted.get_else().get_term() == (
            ddsolve.SmtTerm.mk_int("1") + ddsolve.SmtTerm.mk_int("2")
            * ddsolve.SmtTerm.mk_int_var("x")
        )


class TestSmtNavigation:
    def test_top_comparison(self):
        order = ddsolve.SmtVariableOrder()
        node = make_smt_xadd(order)
        assert repr(node.get_comparison()) == "(not (>= x 5))"
        assert node.get_comparison() == order.get_comparisons()[0]

    def test_then_else(self):
        node = make_smt_xadd(ddsolve.SmtVariableOrder())
        x = ddsolve.SmtTerm.mk_int_var("x")
        assert node.get_then().is_leaf()
        assert node.get_then().get_term() == x
        assert node.get_else().is_leaf()
        assert node.get_else().get_term() == ddsolve.SmtTerm.mk_int("2") * x

    def test_leaf_has_no_top_comparison(self):
        xadd = ddsolve.SmtXADD(ddsolve.SmtVariableOrder())
        assert xadd.get_undefined().get_comparison() is None


class TestSmtMinMax:
    def test_min_max_of_leaf_terms_case_splits(self):
        # Unlike LinearTerm, the Smt two-terminal case works: min/max of the
        # crossing leaves `x` and `2*x` splits on `x < 0`.
        order = ddsolve.SmtVariableOrder()
        xadd = ddsolve.SmtXADD(order)
        x = ddsolve.SmtTerm.mk_int_var("x")
        a = xadd.get_term(x)
        b = xadd.get_term(x + x)
        minimum = a.min(b)
        maximum = a.max(b)
        assert not minimum.is_leaf()
        assert not maximum.is_leaf()
        assert repr(minimum.get_comparison()) == "(not (>= x 0))"
        assert minimum.get_then().get_term() == ddsolve.SmtTerm.mk_int("2") * x
        assert minimum.get_else().get_term() == x
        assert maximum.get_then().get_term() == x
        assert maximum.get_else().get_term() == ddsolve.SmtTerm.mk_int("2") * x

    def test_max_min_on_nodes(self):
        order = ddsolve.SmtVariableOrder()
        xadd = ddsolve.SmtXADD(order)
        x = ddsolve.SmtTerm.mk_int_var("x")
        cond = smt_cond(order)
        f = cond.ite(xadd.get_term(ddsolve.SmtTerm.mk_int("1")),
                     xadd.get_term(ddsolve.SmtTerm.mk_int("0")))
        g = cond.ite(xadd.get_term(ddsolve.SmtTerm.mk_int("3")),
                     xadd.get_term(ddsolve.SmtTerm.mk_int("5")))
        assert f.max(g).get_then().get_term() == ddsolve.SmtTerm.mk_int("3")
        assert f.max(g).get_else().get_term() == ddsolve.SmtTerm.mk_int("5")
        assert f.min(g).get_then().get_term() == ddsolve.SmtTerm.mk_int("1")
        assert f.min(g).get_else().get_term() == ddsolve.SmtTerm.mk_int("0")

    def test_mixed_orders_raise(self):
        a = make_smt_xadd(ddsolve.SmtVariableOrder())
        other = ddsolve.SmtXADD(ddsolve.SmtVariableOrder()).get_term(
            ddsolve.SmtTerm.mk_int("1")
        )
        with pytest.raises(ValueError):
            a + other
        with pytest.raises(ValueError):
            a.max(other)
        with pytest.raises(ValueError):
            a.min(other)


class TestSmtComparisonOps:
    def test_universal_pointwise(self):
        order = ddsolve.SmtVariableOrder()
        xadd = ddsolve.SmtXADD(order)
        x = ddsolve.SmtTerm.mk_int_var("x")
        cond = smt_cond(order)
        f = cond.ite(xadd.get_term(x), xadd.get_term(x + x))
        g = cond.ite(xadd.get_term(ddsolve.SmtTerm.mk_int("3")),
                     xadd.get_term(ddsolve.SmtTerm.mk_int("5")))
        # 1 < 3 and 2*x < 5 do not hold everywhere (large x).
        assert not (f < g)
        assert not (f <= g)
        h = cond.ite(xadd.get_term(ddsolve.SmtTerm.mk_int("3")),
                     xadd.get_term(ddsolve.SmtTerm.mk_int("6")))
        # x < 3 holds only on part of the domain, so still not universal.
        assert not (f < h)

    def test_equality_needs_same_order_object(self):
        a = make_smt_xadd(ddsolve.SmtVariableOrder())
        b = make_smt_xadd(ddsolve.SmtVariableOrder())
        assert a == a
        assert a != b  # order identity, as for XADD

    def test_mixed_orders_raise(self):
        """SmtXADD::operator<, <=, - throw on order mismatch.

        std::invalid_argument is translated to ValueError.
        """
        order1 = ddsolve.SmtVariableOrder()
        order2 = ddsolve.SmtVariableOrder()
        xadd1 = ddsolve.SmtXADD(order1)
        xadd2 = ddsolve.SmtXADD(order2)
        x = ddsolve.SmtTerm.mk_int_var("x")
        a = xadd1.get_term(x)
        b = xadd2.get_term(x)  # different order
        with pytest.raises(ValueError):
            a < b
        with pytest.raises(ValueError):
            a <= b
        with pytest.raises(ValueError):
            a - b


class TestSmtScale:
    def test_scale_node(self):
        node = make_smt_xadd(ddsolve.SmtVariableOrder())
        x = ddsolve.SmtTerm.mk_int_var("x")
        scaled = node.scale("2")
        assert scaled.get_then().get_term() == ddsolve.SmtTerm.mk_int("2") * x
        assert scaled.get_else().get_term() == ddsolve.SmtTerm.mk_int("4") * x

    def test_invalid_scalar_raises(self):
        # Smt scalars are parsed through the SMT solver, which raises
        # RuntimeError (Linear raises ValueError via rational parsing).
        #
        # NOTE: the failed cvc5 input parse corrupts the solver's global
        # incremental state, so every later Smt operation that consults the
        # solver in the same process fails.  The check therefore runs in a
        # subprocess to avoid poisoning the rest of the suite.
        result = subprocess.run(
            [sys.executable, "-c", """
import ddsolve

order = ddsolve.SmtVariableOrder()
cond = ddsolve.SmtXBDD(order).obtain_repr(
    ddsolve.SmtComparison(
        ddsolve.SmtTerm.mk_int_var("x"),
        ddsolve.SmtTerm.mk_int("5"),
        ddsolve.ComparisonType.kLessThan,
    )
)
xadd = ddsolve.SmtXADD(order)
node = cond.ite(xadd.get_term(ddsolve.SmtTerm.mk_int_var("x")),
                xadd.get_term(ddsolve.SmtTerm.mk_int("2")
                              * ddsolve.SmtTerm.mk_int_var("x")))
try:
    node.scale("not-a-number")
except RuntimeError:
    print("RuntimeError")
else:
    raise AssertionError("invalid scalar did not raise")
"""],
            capture_output=True,
            text=True,
            timeout=60,
        )
        assert result.returncode == 0, (
            f"subprocess failed with {result.returncode}:\n{result.stderr}"
        )
        assert result.stdout.strip().splitlines()[-1] == "RuntimeError"


class TestSmtUndefinedToZero:
    def test_replaces_undefined_with_default_term(self):
        # Smt-specific: undefined leaves are replaced by the *default*
        # SmtTerm (an empty term), not an arithmetic zero.
        order = ddsolve.SmtVariableOrder()
        xadd = ddsolve.SmtXADD(order)
        cond = smt_cond(order)
        partial = cond.ite(xadd.get_term(ddsolve.SmtTerm.mk_int_var("x")),
                           xadd.get_undefined())
        total = partial.undefined_to_zero()
        assert not total.get_else().is_false()
        assert total.get_else().get_term() == ddsolve.SmtTerm()


class TestSmtPrune:
    def test_noop_on_node(self):
        node = make_smt_xadd(ddsolve.SmtVariableOrder())
        pruned = node.prune()
        # Pointwise equality instead of syntactic `==`.
        assert pruned <= node and node <= pruned

    def test_returns_xadd_with_same_order(self):
        node = make_smt_xadd(ddsolve.SmtVariableOrder())
        for pruned in (node.prune(), node.prune_full()):
            assert type(pruned) is ddsolve.SmtXADD
            assert pruned.get_order() is node.get_order()


class TestSmtSupportAndPrunes:
    def test_support_of_constant(self):
        xadd = ddsolve.SmtXADD(ddsolve.SmtVariableOrder())
        support = xadd.get_undefined().get_support()
        assert type(support) is ddsolve.SmtXBDD
        assert support.is_leaf()
        assert support.is_true()

    def test_support_of_node(self):
        node = make_smt_xadd(ddsolve.SmtVariableOrder())
        support = node.get_support()
        assert type(support) is ddsolve.SmtXBDD
        assert not support.is_leaf()
        assert support.get_order() is node.get_order()

    def test_prunes_of_node(self):
        node = make_smt_xadd(ddsolve.SmtVariableOrder())
        prunes = node.get_prunes()
        assert type(prunes) is ddsolve.SmtXBDD
        assert prunes.get_order() is node.get_order()


class TestSmtNodeCount:
    def test_counts(self):
        order = ddsolve.SmtVariableOrder()
        node = make_smt_xadd(order)
        assert node.get_nodecount() == 3
        assert ddsolve.SmtXADD(order).get_undefined().get_nodecount() == 1


class TestSmtWithOrder:
    def test_moves_to_new_order(self):
        order = ddsolve.SmtVariableOrder()
        node = make_smt_xadd(order)
        new_order = ddsolve.SmtVariableOrder()
        moved = node.with_order(new_order)
        assert moved.get_order() is new_order
        assert moved.get_comparison() == node.get_comparison()


class TestSmtSubstitute:
    def test_substitute_keeps_same_order(self):
        order = ddsolve.SmtVariableOrder()
        node = make_smt_xadd(order)
        sub = node.substitute(
            ddsolve.SmtTerm.mk_int_var("x"), ddsolve.SmtTerm.mk_int("3")
        )
        # Smt substitution re-derives comparisons without collapsing the
        # diagram, so only structural properties are asserted.
        assert type(sub) is ddsolve.SmtXADD
        assert sub.get_order() is order


class TestSmtSelectorProduct:
    def test_partial_domain_restriction(self):
        # `SmtXBDD * SmtXADD` keeps the XADD value where the condition is
        # true and becomes undefined elsewhere (same as Linear).
        order = ddsolve.SmtVariableOrder()
        cond = smt_cond(order)
        xadd = ddsolve.SmtXADD(order)
        leaf = xadd.get_term(ddsolve.SmtTerm.mk_int_var("x"))
        product = cond * leaf
        assert type(product) is ddsolve.SmtXADD
        assert product == cond.ite(leaf, xadd.get_undefined())

    def test_mixed_orders_raise(self):
        """SmtXBDD * SmtXADD throws on order mismatch."""
        order1 = ddsolve.SmtVariableOrder()
        order2 = ddsolve.SmtVariableOrder()
        cond = ddsolve.SmtXBDD(order1).obtain_repr(
            ddsolve.SmtComparison(
                ddsolve.SmtTerm.mk_int_var("x"),
                ddsolve.SmtTerm.mk_int("5"),
                ddsolve.ComparisonType.kLessThan,
            )
        )
        xadd = ddsolve.SmtXADD(order2)  # different order
        leaf = xadd.get_term(ddsolve.SmtTerm.mk_int_var("x"))
        with pytest.raises(ValueError):
            cond * leaf


class TestSmtToDot:
    def test_returns_dot_string(self):
        node = make_smt_xadd(ddsolve.SmtVariableOrder())
        assert node.to_dot().startswith('digraph "DD"')


# ---------------------------------------------------------------------------
# LinearTerm repr and scale
# ---------------------------------------------------------------------------


class TestLinearTermRepr:
    def test_zero_term(self):
        t = ddsolve.LinearTerm([])
        assert repr(t) == "0"

    def test_single_term(self):
        t = ddsolve.LinearTerm([ddsolve.Product(3, 2)])
        assert repr(t) == "2*x3"

    def test_multi_term(self):
        t = ddsolve.LinearTerm([
            ddsolve.Product(0, 1),
            ddsolve.Product(3, -1),
        ])
        assert repr(t) == "1 + -1*x3"

    def test_rational_coefficients(self):
        t = ddsolve.LinearTerm([
            ddsolve.Product(1, "3/4"),
            ddsolve.Product(2, "1/2"),
        ])
        assert repr(t) == "3/4*x1 + 1/2*x2"

    def test_infinite_term(self):
        t = ddsolve.LinearTerm(ddsolve.FinityType.kPositiveInfinite)
        assert repr(t) == "∞"

    def test_neg_infinite_term(self):
        t = ddsolve.LinearTerm(ddsolve.FinityType.kNegativeInfinite)
        assert repr(t) == "-∞"

    def test_iteration(self):
        """LinearTerm.__iter__ yields the Products in term order."""
        t = ddsolve.LinearTerm([
            ddsolve.Product(1, 3),
            ddsolve.Product(2, "1/2"),
        ])
        products = list(t)
        assert len(products) == 2
        assert products[0].get_variable() == 1
        assert products[1].get_variable() == 2


class TestLinearTermScale:
    def test_scale_by_integer(self):
        t = ddsolve.LinearTerm([ddsolve.Product(3, 2)])
        scaled = t.scale("3")
        assert repr(scaled) == "6*x3"

    def test_scale_by_rational(self):
        t = ddsolve.LinearTerm([ddsolve.Product(1, "2/3")])
        scaled = t.scale("3/2")
        assert repr(scaled) == "1*x1"

    def test_scale_zero(self):
        t = ddsolve.LinearTerm([ddsolve.Product(3, 2), ddsolve.Product(0, 1)])
        scaled = t.scale("0")
        assert repr(scaled) == "0"

    def test_scale_negative(self):
        t = ddsolve.LinearTerm([ddsolve.Product(1, 5)])
        scaled = t.scale("-1")
        assert repr(scaled) == "-5*x1"

    def test_scale_infinite(self):
        t = ddsolve.LinearTerm(ddsolve.FinityType.kPositiveInfinite)
        scaled = t.scale("2")
        # Infinite terms are unchanged by scaling
        assert repr(scaled) == "∞"

    def test_scale_copy_constructed(self):
        t1 = ddsolve.LinearTerm([ddsolve.Product(1, 3)])
        t2 = ddsolve.LinearTerm(t1)  # copy constructor
        scaled = t2.scale("2")
        assert repr(scaled) == "6*x1"


# ---------------------------------------------------------------------------
# LinearComparison repr
# ---------------------------------------------------------------------------


class TestLinearComparisonRepr:
    def _mk(self, products, ctype):
        return ddsolve.LinearComparison(
            ddsolve.LinearTerm([ddsolve.Product(v, c) for v, c in products]),
            ctype,
        )

    def test_equality(self):
        c = self._mk([(3, 1)], ddsolve.ComparisonType.kEquality)
        assert "=" in repr(c)
        assert "0" in repr(c)

    def test_inequality(self):
        c = self._mk([(3, 1)], ddsolve.ComparisonType.kInequality)
        assert "!=" in repr(c)

    def test_less_than(self):
        c = self._mk([(3, -1), (0, 1)], ddsolve.ComparisonType.kLessThan)
        assert "<" in repr(c)

    def test_less_than_or_equal(self):
        c = self._mk([(3, 1)], ddsolve.ComparisonType.kLessThanOrEqual)
        assert "<=" in repr(c)

    def test_greater_than(self):
        c = self._mk([(3, 1)], ddsolve.ComparisonType.kGreaterThan)
        assert ">" in repr(c)

    def test_greater_than_or_equal(self):
        c = self._mk([(3, 1)], ddsolve.ComparisonType.kGreaterThanOrEqual)
        assert ">=" in repr(c)

    def test_two_term_comparison(self):
        lhs = ddsolve.LinearTerm([ddsolve.Product(3, 1)])
        rhs = ddsolve.LinearTerm([ddsolve.Product(0, 5)])
        c = ddsolve.LinearComparison(lhs, rhs, ddsolve.ComparisonType.kLessThan)
        # A two-term comparison is normalized to `rhs - lhs < 0`.
        assert repr(c) == "-5 + 1*x3 < 0"


# ---------------------------------------------------------------------------
# XBDD * XADD (selector product) for LinearTerm
# ---------------------------------------------------------------------------


class TestLinearXBDDMulXADD:
    """Test XBDD * XADD — the selector product, already covered in
    test_xbdd.py but re-asserted here for the XADD-side path."""

    def test_selector_product(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        one = xadd.get_one()
        cond = make_cond(variable_order[0])
        product = cond * one
        undefined = xadd.get_undefined()
        assert product == cond.ite(one, undefined)

    def test_selector_product_with_term(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        five = xadd.get_term(term((0, 5)))
        cond = make_cond(variable_order[0])
        undefined = xadd.get_undefined()
        product = cond * five
        # Selector product: then=5, else=undefined
        assert not product.is_leaf()
        assert product.get_then().get_term() == five.get_term()
        assert product.get_else().get_term() is None



