"""Tests for the Python-exposed methods of `ddsolve.XADD`.

Covers every method/operator bound to the `XADD` class in `src/main.cpp`:
construction, `to_dot`, the constant factories (`get_undefined`, `get_zero`,
`get_one`, `get_infty`, `get_neg_infty`), `get_term` (both overloads),
`get_then`, `get_else`, `get_comparison`, `is_leaf`, `is_true`, `is_false`,
`get_nodecount`, `with_order`, `get_order`, `substitute`, `prune`,
`prune_full`, `max`, `min`, `get_support`, `get_prunes`,
`undefined_to_zero`, `scale`, the comparison operators `==`, `<=`, `<`,
the arithmetic operators `+` and `-`, and `XBDD * XADD` (selector product,
covered in detail in `tests/test_xbdd.py`).

Domain notes (mirroring `tests/test_xbdd.py`):
- `Product(0, c)` is the literal constant `c`; `Product(v, c)` with `v > 0`
  is the term `c * x_v`.  Variables are non-negative integers.
- Comparisons are canonicalized to `kLessThan`; the canonical form of
  `x3 - 1 > 0` is `1 + -1*x3 < 0`.

Known quirks of the C++ layer (asserted as current behavior, not
specification):
- `XADD.__eq__` compares variable-order *identity* plus the underlying
  MTBDD, like `XBDD.__eq__`.
- `prune` / `prune_full` and `get_prunes` semantics on diagrams with
  undefined leaves are murky, so those are only checked lightly.
- `get_neg_infty` is bound incorrectly (it also produces positive
  infinity) — see `tests/test_xadd_quirks.py`.
"""

import pytest

import ddsolve

GT = ddsolve.ComparisonType.kGreaterThan


def term(*products):
    """Build a LinearTerm from (variable, coefficient) pairs."""
    return ddsolve.LinearTerm([ddsolve.Product(v, c) for v, c in products])


def gt_one(var=3, k=1):
    """The comparison `x_var - k > 0` (non-trivially decidable)."""
    return ddsolve.LinearComparison(term((var, 1), (0, -k)), GT)


def make_cond(order):
    """The XBDD condition `x3 - 1 > 0` (canonicalized to `1 - x3 < 0`)."""
    return ddsolve.XBDD(order).obtain_repr(gt_one())


def make_node(order):
    """The XADD `(x3 - 1 > 0) ? 1 : 0` — then leaf `1`, else leaf `0`."""
    xadd = ddsolve.XADD(order)
    cond = make_cond(order)
    return cond.ite(xadd.get_one(), xadd.get_zero())


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
        assert one + one == two
        assert two - one == one

    def test_add_node(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        node = make_node(variable_order[0])
        shifted = node + xadd.get_one()
        assert shifted.get_then().get_term() == term((0, 2))
        assert shifted.get_else().get_term() == term((0, 1))

    def test_add_leaf_terms(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        a = xadd.get_term(term((0, 1), (3, -1)))   # 1 - x3/10... c=-1 here
        b = xadd.get_term(term((3, 1)))
        # (1 - x3) + x3 = 1
        assert a + b == xadd.get_one()

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
        assert one.max(two) == two
        assert two.min(one) == one

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
        assert two.scale("1/2") == xadd.get_one()

    def test_scale_leaf_term(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        a = xadd.get_term(term((0, 1), (3, -1)))
        scaled = a.scale("2")
        assert scaled == xadd.get_term(term((0, 2), (3, -2)))

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
        assert total == cond.ite(one, xadd.get_zero())
        assert total.get_else().get_term() == ddsolve.LinearTerm([])
        assert not total.get_else().is_false()

    def test_noop_without_undefined(self, variable_order):
        node = make_node(variable_order[0])
        assert node.undefined_to_zero() == node


class TestPrune:
    def test_noop_on_fully_defined(self, variable_order):
        node = make_node(variable_order[0])
        assert node.prune() == node
        assert node.prune_full() == node

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
        assert cond.ite(zero, zero) == zero


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
        assert node.substitute(3, term((3, 1))) == node

    def test_substitute_rewrites_leaf_terms(self, variable_order):
        # Substitution reaches *inside* leaf terms: 1 - x3 with x3 := x3/20
        # becomes 1 - x3/200.
        xadd = ddsolve.XADD(variable_order[0])
        a = xadd.get_term(term((0, 1), (3, -1)))
        sub = a.substitute(3, term((3, 1)))
        expected = xadd.get_term(term((0, 1), (3, -1)))
        # Identity substitution keeps the term.
        assert sub == expected
        # A real rewrite: x3 := 2*x3.
        sub2 = a.substitute(3, term((3, 2)))
        assert sub2 == xadd.get_term(term((0, 1), (3, -2)))


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


class TestToDot:
    def test_returns_dot_string(self, variable_order):
        node = make_node(variable_order[0])
        dot = node.to_dot()
        assert isinstance(dot, str)
        assert dot.startswith('digraph "DD"')

    def test_leaf_dot(self, variable_order):
        xadd = ddsolve.XADD(variable_order[0])
        assert xadd.get_one().to_dot().startswith('digraph "DD"')
