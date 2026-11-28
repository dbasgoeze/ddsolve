"""Tests for XBDD operators: ~, &, |, ite, ==, and the selector product
(XBDD * XADD).  See tests/test_xbdd.py for construction/obtain_repr."""

import pytest

import ddsolve
from helpers import GT, LT, gt_one, make_cond, term

# A non-leaf XBDD encoding `x3 - 1 > 0`, canonicalized to `1 - x3 < 0`.
make_node = make_cond


def canonical_comp():
    """The canonical `kLessThan` form of `x3 - 1 > 0`."""
    return ddsolve.LinearComparison(term((0, 1), (3, -1)), LT)


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

    def test_mixed_orders_raise(self):
        """operator*(XBDD, XADD) throws on order mismatch."""
        order1 = ddsolve.VariableOrder()
        order2 = ddsolve.VariableOrder()
        node = make_node(order1)
        xadd = ddsolve.XADD(order2)  # different order
        one = xadd.get_one()
        with pytest.raises(ValueError):
            node * one

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
