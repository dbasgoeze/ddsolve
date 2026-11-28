"""Tests for combining XBDD conditions with XADD leaves: ite and the
selector product (XBDD * XADD, Linear-side path)."""

import pytest

import ddsolve
from helpers import make_cond, term


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
