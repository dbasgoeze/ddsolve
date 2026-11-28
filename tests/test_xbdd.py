"""Tests for the Python-exposed methods of `ddsolve.XBDD`.

Domain notes (shared with the XADD test files):
- `Product(0, c)` represents the literal constant `c`; `Product(v, c)` with
  `v > 0` is the term `c * x_v`.  Variables are non-negative integers, so
  some comparisons are decided trivially (e.g. `x3 < 0` is unsat) without
  creating a diagram node.
- Only canonical `kLessThan` comparisons are registered as sylvan variables;
  `obtain_repr` canonicalizes other comparison types (e.g. `t > 0` becomes
  `-t < 0`).

Known quirks of the C++ layer (asserted as current behavior, not
specification):
- `XBDD.__eq__` compares variable-order *identity* plus the underlying
  MTBDD, so structurally identical diagrams built from two different
  `VariableOrder` objects compare unequal.
- `XBDD * XADD` is an intentional partial-domain restriction: paths where
  the boolean diagram is false become undefined (`mtbdd_false`).
- `Support()` on the MTBDD representation has murky semantics for
  non-boolean leaves, so `get_support` is only checked lightly for nodes.
"""

import ddsolve
from helpers import GT, LT, gt_one, make_cond, term

# A non-leaf XBDD encoding `x3 - 1 > 0`, canonicalized to `1 - x3 < 0`.
make_node = make_cond


def canonical_comp():
    """The canonical `kLessThan` form of `x3 - 1 > 0`."""
    return ddsolve.LinearComparison(term((0, 1), (3, -1)), LT)


class TestConstruction:
    def test_with_order(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        assert bdd.get_order() is variable_order[0]

    def test_default(self):
        bdd = ddsolve.XBDD()
        assert isinstance(bdd.get_order(), ddsolve.VariableOrder)
        # An empty diagram is a false leaf.
        assert bdd.is_leaf()
        assert not bdd.is_true()
        assert bdd.is_false()


class TestConstants:
    def test_true(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        t = bdd.get_true()
        assert t.is_leaf()
        assert t.is_true()
        assert not t.is_false()

    def test_false(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        f = bdd.get_false()
        assert f.is_leaf()
        assert not f.is_true()
        assert f.is_false()

    def test_negation_of_constants(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        t, f = bdd.get_true(), bdd.get_false()
        assert ~t == f
        assert ~f == t
        assert t != f

    def test_constants_share_order(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        assert bdd.get_true().get_order() is variable_order[0]
        assert bdd.get_false().get_order() is variable_order[0]


class TestObtainRepr:
    def test_non_trivial_comparison(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        node = bdd.obtain_repr(gt_one(3))
        assert not node.is_leaf()
        assert not node.is_true()
        assert not node.is_false()

    def test_canonicalizes_to_less_than(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        node = bdd.obtain_repr(gt_one(3))
        # Only the canonical `kLessThan` form is registered.
        assert variable_order[0].get_comparisons() == [canonical_comp()]
        assert node.get_comparison() == canonical_comp()

    def test_canonical_input_stored_as_is(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        comp = ddsolve.LinearComparison(term((3, -1)), LT)  # -x3 < 0
        node = bdd.obtain_repr(comp)
        assert not node.is_leaf()
        assert node.get_comparison() == comp
        assert variable_order[0].get_comparisons() == [comp]

    def test_trivially_unsat_comparison(self, variable_order):
        # Variables are non-negative, so `x3 < 0` is decided without a node.
        bdd = ddsolve.XBDD(variable_order[0])
        node = bdd.obtain_repr(ddsolve.LinearComparison(term((3, 1)), LT))
        assert node.is_leaf()
        assert node.is_false()

    def test_trivially_sat_comparison(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        node = bdd.obtain_repr(
            ddsolve.LinearComparison(term((0, 1)), GT)  # 1 > 0
        )
        assert node.is_leaf()
        assert node.is_true()

    def test_trivial_finity_comparisons(self, variable_order):
        # `trivialFinityCheck`: comparisons over infinite terms decide
        # immediately and never register a diagram node.
        bdd = ddsolve.XBDD(variable_order[0])
        pos_inf = ddsolve.LinearTerm(ddsolve.FinityType.kPositiveInfinite)
        neg_inf = ddsolve.LinearTerm(ddsolve.FinityType.kNegativeInfinite)
        CT = ddsolve.ComparisonType
        sat = [(pos_inf, CT.kGreaterThan),
               (pos_inf, CT.kGreaterThanOrEqual),
               (pos_inf, CT.kInequality),
               (neg_inf, CT.kLessThan),
               (neg_inf, CT.kLessThanOrEqual),
               (neg_inf, CT.kInequality)]
        unsat = [(pos_inf, CT.kLessThan),
                 (pos_inf, CT.kLessThanOrEqual),
                 (pos_inf, CT.kEquality),
                 (neg_inf, CT.kGreaterThan),
                 (neg_inf, CT.kGreaterThanOrEqual),
                 (neg_inf, CT.kEquality)]
        for finity, comp_type in sat:
            node = bdd.obtain_repr(ddsolve.LinearComparison(finity, comp_type))
            assert node.is_leaf() and node.is_true(), (finity, comp_type)
        for finity, comp_type in unsat:
            node = bdd.obtain_repr(ddsolve.LinearComparison(finity, comp_type))
            assert node.is_leaf() and node.is_false(), (finity, comp_type)
        assert variable_order[0].get_comparisons() == []

    def test_trivial_constant_comparisons(self, variable_order):
        # `trivialConstantCheck`: terms without variables decide by
        # evaluating the constant carrier product directly.
        bdd = ddsolve.XBDD(variable_order[0])
        CT = ddsolve.ComparisonType
        sat = [(0, CT.kEquality), (5, CT.kInequality), (-1, CT.kLessThan),
               (0, CT.kLessThanOrEqual), (5, CT.kGreaterThan),
               (0, CT.kGreaterThanOrEqual)]
        unsat = [(5, CT.kEquality), (0, CT.kInequality), (5, CT.kLessThan),
                 (5, CT.kLessThanOrEqual), (-1, CT.kGreaterThan),
                 (-1, CT.kGreaterThanOrEqual)]
        for c, comp_type in sat:
            comp = ddsolve.LinearComparison(term((0, c)), comp_type)
            node = bdd.obtain_repr(comp)
            assert node.is_leaf() and node.is_true(), (c, comp_type)
        for c, comp_type in unsat:
            comp = ddsolve.LinearComparison(term((0, c)), comp_type)
            node = bdd.obtain_repr(comp)
            assert node.is_leaf() and node.is_false(), (c, comp_type)
        # The empty term has no constant carrier product at all.
        empty = ddsolve.LinearTerm([])
        for comp_type in (CT.kEquality, CT.kLessThanOrEqual,
                          CT.kGreaterThanOrEqual):
            node = bdd.obtain_repr(ddsolve.LinearComparison(empty, comp_type))
            assert node.is_leaf() and node.is_true(), comp_type
        for comp_type in (CT.kInequality, CT.kLessThan, CT.kGreaterThan):
            node = bdd.obtain_repr(ddsolve.LinearComparison(empty, comp_type))
            assert node.is_leaf() and node.is_false(), comp_type
        assert variable_order[0].get_comparisons() == []

    def test_trivial_less_than_shapes(self, variable_order):
        # `trivialLessThanCheck` only fires for `kLessThan` comparisons.
        bdd = ddsolve.XBDD(variable_order[0])
        # All coefficients >= 0 and no negative constant: unsat everywhere.
        assert bdd.obtain_repr(
            ddsolve.LinearComparison(term((3, 1)), LT)).is_false()
        # All coefficients <= 0 with a leading negative constant: sat
        # everywhere (the constant is needed so the term cannot be 0).
        assert bdd.obtain_repr(
            ddsolve.LinearComparison(term((0, -1), (3, -1)), LT)).is_true()
        # Mixed signs: not trivially decidable, registered as a node.
        mixed = ddsolve.LinearComparison(term((3, 1), (0, -1)), LT)
        node = bdd.obtain_repr(mixed)
        assert not node.is_leaf()
        assert node.get_comparison() == mixed
        # Non-`kLessThan` types skip the check entirely; `x3 <= 0`
        # canonicalizes to the negation of the non-trivial `-x3 < 0` node.
        node = bdd.obtain_repr(
            ddsolve.LinearComparison(term((3, 1)),
                                     ddsolve.ComparisonType.kLessThanOrEqual)
        )
        assert not node.is_leaf()
        assert not node.is_true() and not node.is_false()

    def test_registers_sylvan_variable(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        bdd.obtain_repr(gt_one(3))
        assert variable_order[0].get_variables() == [0]

    def test_leaf_has_no_top_comparison(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        assert bdd.get_true().get_comparison() is None


class TestNavigation:
    def test_then_else_of_comparison_node(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        node = bdd.obtain_repr(gt_one(3))
        then_, else_ = node.get_then(), node.get_else()
        # `x3 - 1 > 0` is satisfiable, so then is the true leaf and else the
        # false leaf.
        assert then_.is_leaf() and then_.is_true()
        assert else_.is_leaf() and else_.is_false()
        assert then_.get_order() is variable_order[0]
        assert else_.get_order() is variable_order[0]

    def test_top_comparison(self, variable_order):
        bdd = ddsolve.XBDD(variable_order[0])
        node = bdd.obtain_repr(gt_one(3))
        assert node.get_comparison() == canonical_comp()
        assert node.get_comparison() == variable_order[0].get_comparisons()[0]
