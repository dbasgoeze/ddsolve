"""Tests for SmtXBDD: same interface as XBDD, with SmtTerm comparisons.
See the Linear-side files for the XBDD counterparts."""

import pytest

import ddsolve
from helpers import smt_cond


# ---------------------------------------------------------------------------
# SmtXBDD — same interface, SmtTerm leaves and comparisons.
# ---------------------------------------------------------------------------


def smt_x_less_than(y):
    """The non-trivially-decidable comparison `x < y` as a term pair."""
    x = ddsolve.SmtTerm.mk_int_var("x")
    return ddsolve.SmtComparison(x, y, ddsolve.ComparisonType.kLessThan)


# The SmtXBDD condition `x < 5`, canonicalized to `(not (>= x 5))`.
make_smt_cond = smt_cond


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

    def test_mixed_orders_raise(self):
        """SmtXBDD::ite(SmtXADD, SmtXADD) throws on order mismatch."""
        order1 = ddsolve.SmtVariableOrder()
        order2 = ddsolve.SmtVariableOrder()
        cond = make_smt_cond(order1)
        xadd1 = ddsolve.SmtXADD(order1)
        xadd2 = ddsolve.SmtXADD(order2)
        then_ = xadd1.get_term(ddsolve.SmtTerm.mk_int("1"))
        else_ = xadd1.get_term(ddsolve.SmtTerm.mk_int("0"))
        wrong_then = xadd2.get_term(ddsolve.SmtTerm.mk_int("1"))
        with pytest.raises(ValueError):
            cond.ite(wrong_then, else_)
        with pytest.raises(ValueError):
            cond.ite(then_, wrong_then)


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
