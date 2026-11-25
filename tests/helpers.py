"""Builders shared by the XBDD / XADD / Smt test files.

Variables are non-negative integers; `Product(0, c)` is the literal
constant `c` and `Product(v, c)` with `v > 0` is the term `c * x_v`.
"""

import ddsolve

GT = ddsolve.ComparisonType.kGreaterThan
LT = ddsolve.ComparisonType.kLessThan


def term(*products):
    """Build a LinearTerm from (variable, coefficient) pairs."""
    return ddsolve.LinearTerm([ddsolve.Product(v, c) for v, c in products])


def gt_one(var, k=1):
    """The comparison `x_var - k > 0` (non-trivially decidable)."""
    return ddsolve.LinearComparison(term((var, 1), (0, -k)), GT)


def make_cond(order):
    """The XBDD condition `x3 - 1 > 0` (canonicalized to `1 - x3 < 0`)."""
    return ddsolve.XBDD(order).obtain_repr(gt_one(3))


def pointwise_equal(f, g):
    """Semantic equality of two diagrams: `f <= g` and `f >= g` both hold.

    (`XADD.__eq__` itself compares variable-order identity plus the
    underlying MTBDD, not diagram-level semantics.)
    """
    return bool((f <= g) and (f >= g))


def smt_int_comp(k=5):
    """The comparison `x < k` over an integer variable `x`."""
    return ddsolve.SmtComparison(ddsolve.SmtTerm.mk_int_var("x"),
                                 ddsolve.SmtTerm.mk_int(str(k)), LT)


def smt_cond(order, k=5):
    """The SmtXBDD condition `x < k`, canonicalized to `(not (>= x k))`."""
    return ddsolve.SmtXBDD(order).obtain_repr(smt_int_comp(k))


def make_smt_xadd(order):
    """The SmtXADD `(x < 5) ? x : 2*x` over the given order."""
    xadd = ddsolve.SmtXADD(order)
    x = ddsolve.SmtTerm.mk_int_var("x")
    return smt_cond(order).ite(xadd.get_term(x), xadd.get_term(x + x))
