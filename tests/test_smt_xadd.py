"""Tests for SmtXADD: same interface as XADD, with SmtTerm leaves and
comparisons.  See the Linear-side files for the XADD counterparts."""

import subprocess
import sys

import pytest

import ddsolve
from helpers import make_smt_xadd, smt_cond


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
