"""Tests for the Python-exposed methods of `ddsolve.XBDD`.

Covers every method/operator bound to the `XBDD` class in `src/main.cpp`:
construction, `to_dot`, `get_true`, `get_false`, `obtain_repr`, `get_then`,
`get_else`, `get_comparison`, `is_leaf`, `is_true`, `is_false`, `with_order`,
`get_order`, `substitute`, `get_support`, `ite` (XBDD and XADD overloads),
`==`, `&`, `|` and `~`.

Domain notes:
- `Product(0, c)` represents the literal constant `c`; variable id 0 is the
  constant carrier, `Product(v, c)` with `v > 0` is the term `c * x_v`.
- Variables are non-negative integers, so some comparisons are decided
  trivially (e.g. `x3 < 0` is unsat) without creating a diagram node.
- Only canonical `kLessThan` comparisons are registered as sylvan variables;
  `obtain_repr` canonicalizes other comparison types (e.g. `t > 0` becomes
  `-t < 0`).

Known quirks of the C++ layer (asserted as current behavior, not
specification):
- `XBDD.__eq__` compares variable-order *identity* plus the underlying
  MTBDD, so structurally identical diagrams built from two different
  `VariableOrder` objects compare unequal.
- `XBDD * XADD` is an intentional partial-domain restriction: paths where
  the boolean diagram is false become undefined (`mtbdd_false`), so the
  result is not the total `ite(bdd, add, zero)`.
- `Support()` on the MTBDD representation has murky semantics for
  non-boolean leaves, so `get_support` is only checked lightly for nodes.
- For Smt diagrams: trivially decided comparisons (`x < x + 1`) do *not*
  collapse to a constant leaf — they become a decision node over a
  canonicalized `"true"` comparison (Linear collapses instead).

SmtXBDD is covered by the `TestSmt*` classes at the end of this file.
"""

import pytest

import ddsolve

GT = ddsolve.ComparisonType.kGreaterThan
LT = ddsolve.ComparisonType.kLessThan


def term(*products):
    """Build a LinearTerm from (variable, coefficient) pairs."""
    return ddsolve.LinearTerm([ddsolve.Product(v, c) for v, c in products])


def gt_one(var, k=1):
    """The comparison `x_var - k > 0` (non-trivially decidable)."""
    return ddsolve.LinearComparison(term((var, 1), (0, -k)), GT)


def make_node(order):
    """A non-leaf XBDD encoding `x3 - 1 > 0`, canonicalized to `1 - x3 < 0`."""
    return ddsolve.XBDD(order).obtain_repr(gt_one(3))


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


class TestBooleanOps:
    def test_negation(self, variable_order):
        node = make_node(variable_order[0])
        neg = ~node
        assert neg.get_then().is_false()
        assert neg.get_else().is_true()

    def test_double_negation(self, variable_order):
        node = make_node(variable_order[0])
        assert ~~node == node

    def test_contradiction_and_tautology(self, variable_order):
        node = make_node(variable_order[0])
        assert (node & ~node).is_false()
        assert (node | ~node).is_true()

    def test_absorption_of_constants(self, variable_order):
        node = make_node(variable_order[0])
        t, f = node.get_true(), node.get_false()
        assert (node & t) == node
        assert (node & f).is_false()
        assert (node | t).is_true()
        assert (node | f) == node

    def test_equality(self, variable_order):
        node = make_node(variable_order[0])
        assert node == node
        assert node != ~node

    def test_equality_needs_same_order_object(self):
        # Documented quirk: equality compares order identity, so two
        # structurally identical diagrams with distinct orders differ.
        comp = gt_one(3)
        a = ddsolve.XBDD(ddsolve.VariableOrder()).obtain_repr(comp)
        b = ddsolve.XBDD(ddsolve.VariableOrder()).obtain_repr(comp)
        assert a != b

    @pytest.mark.parametrize("op", ["and", "or", "ite"])
    def test_mixed_orders_raise(self, op):
        a = ddsolve.XBDD(ddsolve.VariableOrder()).obtain_repr(gt_one(3))
        b = ddsolve.XBDD(ddsolve.VariableOrder()).obtain_repr(gt_one(3))
        t, f = b.get_true(), b.get_false()  # branches from a different order
        with pytest.raises(ValueError):
            if op == "and":
                a & b
            elif op == "or":
                a | b
            else:
                a.ite(t, f)


class TestIte:
    def test_ite_identity(self, variable_order):
        node = make_node(variable_order[0])
        t, f = node.get_true(), node.get_false()
        assert node.ite(t, f) == node

    def test_ite_inversion(self, variable_order):
        node = make_node(variable_order[0])
        t, f = node.get_true(), node.get_false()
        assert node.ite(f, t) == ~node

    def test_ite_self(self, variable_order):
        # ite(r, r, ~r) is the tautology r \<or> ~r.
        node = make_node(variable_order[0])
        assert node.ite(node, ~node).is_true()

    def test_ite_with_xadd_branches(self, variable_order):
        node = make_node(variable_order[0])
        xadd = ddsolve.XADD(variable_order[0])
        one, zero = xadd.get_one(), xadd.get_zero()
        result = node.ite(one, zero)
        assert type(result) is ddsolve.XADD
        # The diagram selects `one` where the condition holds, `zero` where
        # it does not.
        assert result.get_then().is_leaf()
        assert result.get_then().get_term() == one.get_term()
        assert result.get_else().is_leaf()
        assert result.get_else().get_term() == zero.get_term()


class TestSelectorProduct:
    """`XBDD * XADD` — an intentional partial-domain restriction.

    The product keeps the XADD leaf on paths where the boolean diagram is
    true and becomes undefined (`mtbdd_false`) where it is false — i.e.
    `ite(bdd, add, undefined)`, not the total `ite(bdd, add, zero)`.
    """

    def test_is_partial_domain_restriction(self, variable_order):
        node = make_node(variable_order[0])
        xadd = ddsolve.XADD(variable_order[0])
        one, zero = xadd.get_one(), xadd.get_zero()
        undefined = xadd.get_undefined()
        product = node * one
        assert product == node.ite(one, undefined)
        assert product != node.ite(one, zero)

    def test_children_reachable(self, variable_order):
        node = make_node(variable_order[0])
        xadd = ddsolve.XADD(variable_order[0])
        one = xadd.get_one()
        product = node * one
        then_ = product.get_then()
        else_ = product.get_else()
        # The then leaf is the XADD leaf (`1`); the else leaf is the
        # undefined one, which carries no term.
        assert then_.is_leaf() and else_.is_leaf()
        assert then_.get_term() == one.get_term()
        assert else_.is_false()
        assert else_.get_term() is None


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


# ---------------------------------------------------------------------------
# SmtXBDD — same interface, SmtTerm leaves and comparisons.
# ---------------------------------------------------------------------------


def smt_x_less_than(y):
    """The non-trivially-decidable comparison `x < y` as a term pair."""
    x = ddsolve.SmtTerm.mk_int_var("x")
    return ddsolve.SmtComparison(x, y, ddsolve.ComparisonType.kLessThan)


def make_smt_cond(order):
    """The SmtXBDD condition `x < 5`, canonicalized to `(not (>= x 5))`."""
    return ddsolve.SmtXBDD(order).obtain_repr(
        smt_x_less_than(ddsolve.SmtTerm.mk_int("5"))
    )


class TestSmtConstruction:
    def test_with_order(self):
        order = ddsolve.SmtVariableOrder()
        bdd = ddsolve.SmtXBDD(order)
        assert bdd.get_order() is order

    def test_default(self):
        bdd = ddsolve.SmtXBDD()
        assert isinstance(bdd.get_order(), ddsolve.SmtVariableOrder)
        assert bdd.is_leaf()
        assert not bdd.is_true()
        assert bdd.is_false()


class TestSmtConstants:
    def test_true_false(self):
        bdd = ddsolve.SmtXBDD(ddsolve.SmtVariableOrder())
        t, f = bdd.get_true(), bdd.get_false()
        assert t.is_leaf() and t.is_true() and not t.is_false()
        assert f.is_leaf() and not f.is_true() and f.is_false()
        assert ~t == f
        assert ~f == t
        assert t != f
        assert t.get_order() is f.get_order()


class TestSmtObtainRepr:
    def test_non_trivial_comparison(self):
        order = ddsolve.SmtVariableOrder()
        bdd = ddsolve.SmtXBDD(order)
        node = make_smt_cond(order)
        assert not node.is_leaf()
        assert not node.is_true()
        assert not node.is_false()
        # Canonicalized to `(not (>= x 5))` and registered in the order.
        assert repr(node.get_comparison()) == "(not (>= x 5))"
        assert node.get_comparison() == order.get_comparisons()[0]

    def test_then_else(self):
        order = ddsolve.SmtVariableOrder()
        node = make_smt_cond(order)
        then_, else_ = node.get_then(), node.get_else()
        assert then_.is_leaf() and then_.is_true()
        assert else_.is_leaf() and else_.is_false()
        assert then_.get_order() is order
        assert else_.get_order() is order

    def test_trivial_comparison_stays_a_node(self):
        # Smt-specific: a trivially decided comparison does not collapse to
        # a constant; it becomes a decision over a canonical `"true"`
        # comparison that is registered in the order (Linear collapses).
        order = ddsolve.SmtVariableOrder()
        bdd = ddsolve.SmtXBDD(order)
        node = bdd.obtain_repr(
            ddsolve.SmtComparison(
                ddsolve.SmtTerm.mk_int_var("x"),
                ddsolve.SmtTerm.mk_int_var("x")
                + ddsolve.SmtTerm.mk_int("1"),
                ddsolve.ComparisonType.kLessThan,
            )
        )
        assert not node.is_leaf()
        assert repr(node.get_comparison()) == "true"
        assert node.get_then().is_leaf() and node.get_then().is_true()
        assert node.get_else().is_leaf() and node.get_else().is_false()

    def test_leaf_has_no_top_comparison(self):
        bdd = ddsolve.SmtXBDD(ddsolve.SmtVariableOrder())
        assert bdd.get_true().get_comparison() is None


class TestSmtBooleanOps:
    def test_negation_and_identities(self):
        node = make_smt_cond(ddsolve.SmtVariableOrder())
        neg = ~node
        assert neg.get_then().is_false()
        assert neg.get_else().is_true()
        assert ~~node == node
        assert (node & ~node).is_false()
        assert (node | ~node).is_true()
        assert node == node
        assert node != ~node

    def test_equality_needs_same_order_object(self):
        comp = smt_x_less_than(ddsolve.SmtTerm.mk_int("5"))
        a = ddsolve.SmtXBDD(ddsolve.SmtVariableOrder()).obtain_repr(comp)
        b = ddsolve.SmtXBDD(ddsolve.SmtVariableOrder()).obtain_repr(comp)
        assert a != b  # order identity is part of equality, as for XBDD

    def test_mixed_orders_raise(self):
        a = make_smt_cond(ddsolve.SmtVariableOrder())
        b = make_smt_cond(ddsolve.SmtVariableOrder())
        t, f = b.get_true(), b.get_false()
        with pytest.raises(ValueError):
            a & b
        with pytest.raises(ValueError):
            a | b
        with pytest.raises(ValueError):
            a.ite(t, f)


class TestSmtIte:
    def test_identity_and_inversion(self):
        node = make_smt_cond(ddsolve.SmtVariableOrder())
        t, f = node.get_true(), node.get_false()
        assert node.ite(t, f) == node
        assert node.ite(f, t) == ~node


class TestSmtWithOrder:
    def test_moves_to_new_order(self):
        order = ddsolve.SmtVariableOrder()
        node = make_smt_cond(order)
        new_order = ddsolve.SmtVariableOrder()
        moved = node.with_order(new_order)
        assert moved.get_order() is new_order
        assert moved.get_order() is not order
        assert moved.get_comparison() == node.get_comparison()


class TestSmtSubstitute:
    def test_substitute_keeps_same_order(self):
        order = ddsolve.SmtVariableOrder()
        node = make_smt_cond(order)
        sub = node.substitute(
            ddsolve.SmtTerm.mk_int_var("x"), ddsolve.SmtTerm.mk_int("3")
        )
        # Smt substitution re-derives comparisons but leaves the diagram a
        # node (the substituted condition is decided over a canonical
        # comparison), so only structural properties are asserted.
        assert type(sub) is ddsolve.SmtXBDD
        assert sub.get_order() is order


class TestSmtSupport:
    def test_support_of_constant(self):
        bdd = ddsolve.SmtXBDD(ddsolve.SmtVariableOrder())
        support = bdd.get_true().get_support()
        assert support.is_leaf()
        assert support.is_true()

    def test_support_of_node(self):
        node = make_smt_cond(ddsolve.SmtVariableOrder())
        support = node.get_support()
        assert type(support) is ddsolve.SmtXBDD
        assert not support.is_leaf()
        assert support.get_order() is node.get_order()


class TestSmtToDot:
    def test_returns_dot_string(self):
        node = make_smt_cond(ddsolve.SmtVariableOrder())
        assert node.to_dot().startswith('digraph "DD"')
