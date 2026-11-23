"""Known-bug tests for C++ quirks of the Python-exposed XADD interface.

These tests encode the *desired* behavior.  They turn green once the
underlying bug is fixed — at that point the fix is confirmed and these
assertions can be relocated to the regular suite.  Tests whose bugs are
still open are marked `xfail(strict=True)` so they stay visible without
breaking the suite.

Quirks covered:

1. `XADD.get_neg_infty` (binding in `src/main.cpp`) constructs its term with
   `FinityType::kPositiveInfinite`, the same constant as `get_infty`, so it
   returns positive infinity instead of negative infinity.

2. `XADD.min` / `XADD.max` used to lose information when both operands were
   leaf diagrams carrying *distinct* linear terms (collapsing to a single
   leaf, silently discarding the other operand).  Since the sylvan gather
   fix they case-split on the crossing condition; only the canonical form
   deviates from Algorithm 4 (flipped comparison with swapped branches —
   see `TestMinMaxOfCrossingLeaves`).

The test bodies run in subprocesses: a failing assertion inside the child
would otherwise pin the traceback in the parent (keeping the C++ objects
alive), tripping the Sylvan ref-count leak-guard in `tests/conftest.py`.
"""

import pytest
import subprocess
import sys

BODY_PREAMBLE = """\
import ddsolve._ddsolve as D


def term(cs):
    #  {var: coef}, var 0 = constant
    return D.LinearTerm([D.Product(v, c) for v, c in cs.items()])


def make_leaf(xa, c_then, c_else):
    #  (x1 > 1) ? c_then : c_else in xa's order
    order = xa.get_order()
    cond = D.XBDD(order).obtain_repr(
        D.LinearComparison(term({0: "1", 1: "-1"}),
                           D.ComparisonType.kLessThan))
    return cond.ite(xa.get_term(term({0: c_then})),
                    xa.get_term(term({0: c_else})))


order = D.VariableOrder()
xa = D.XADD(order)

# A = 1 - x1/10 and B = x1/20 cross at x1 = 20/3.
A = xa.get_term(term({0: "1", 1: "-1/10"}))
B = xa.get_term(term({1: "1/20"}))

# The condition A < B, i.e. 1 - 3*x1/20 < 0, canonicalized by the order.
xbdd = D.XBDD(order)
cond = xbdd.obtain_repr(
    D.LinearComparison(term({0: "1", 1: "-3/20"}), D.ComparisonType.kLessThan)
)
"""


def run_child(body):
    result = subprocess.run(
        [sys.executable, "-c", BODY_PREAMBLE + body],
        capture_output=True,
        text=True,
        timeout=60,
    )
    # graphviz is optional; its import-time warning would otherwise pollute
    # the stdout assertions below.
    out = "\n".join(
        line for line in result.stdout.splitlines()
        if "Missing Dependency" not in line
    )
    return result.returncode, out, result.stderr


@pytest.mark.xfail(strict=True, reason="get_neg_infty returns +inf "
                    "(src/main.cpp binding uses kPositiveInfinite)")
def test_neg_infty_is_negative():
    code, out, err = run_child("""
xadd = D.XADD(order)
neg_infty = xadd.get_neg_infty()
infty = xadd.get_infty()

assert neg_infty.get_term() == D.LinearTerm(
    D.FinityType.kNegativeInfinite
), f"term is {neg_infty.get_term()!r}"
assert neg_infty != infty, "negative infinity equals positive infinity"
print("neg_infty ok")
""")
    assert code == 0, f"subprocess failed with {code}:\n{err}"
    assert out.strip() == "neg_infty ok"


class TestMinMaxOfCrossingLeaves:
    """min/max of two leaf diagrams with distinct linear terms.

    Both case-split on the crossing condition, but on the *flipped*
    comparison (`x1 < 20/3`, i.e. `not (A < B)`-form) instead of Algorithm 4's
    literal `Obtain(L_T(X1) < L_T(X2), X1, X2)`; the branches are swapped
    accordingly.  The result is semantically identical — min picks the
    smaller, max the larger term on each side of the crossing — only the
    canonical form differs from the algorithm's wording.
    """

    def test_min_case_splits(self):
        code, out, err = run_child("""
cond_flip = xbdd.obtain_repr(
    D.LinearComparison(term({0: "-1", 1: "3/20"}),
                       D.ComparisonType.kLessThan))
minimum = A.min(B)
assert not minimum.is_leaf(), f"min collapsed to leaf: {minimum.get_term()!r}"
# Pointwise equality (wp.py `==` idiom): `f <= g and f >= g`, not the
# syntactic `XADD.__eq__`.
expected = cond_flip.ite(B, A)
assert minimum <= expected and expected <= minimum, "min is not ite(x1 < 20/3, B, A)"
assert minimum.get_then().get_term() == B.get_term()
assert minimum.get_else().get_term() == A.get_term()
print("min ok")
""")
        assert code == 0, f"subprocess failed with {code}:\n{err}"
        assert out.strip() == "min ok"

    def test_max_case_splits(self):
        code, out, err = run_child("""
cond_flip = xbdd.obtain_repr(
    D.LinearComparison(term({0: "-1", 1: "3/20"}),
                       D.ComparisonType.kLessThan))
maximum = A.max(B)
assert not maximum.is_leaf(), f"max collapsed to leaf: {maximum.get_term()!r}"
expected = cond_flip.ite(A, B)
assert maximum <= expected and expected <= maximum, "max is not ite(x1 < 20/3, A, B)"
assert maximum.get_then().get_term() == A.get_term()
assert maximum.get_else().get_term() == B.get_term()
print("max ok")
""")
        assert code == 0, f"subprocess failed with {code}:\n{err}"
        assert out.strip() == "max ok"


@pytest.mark.xfail(strict=True, reason="operator</<= on XADD lack the "
                    "mixed-order check (src/XADD.cpp)")
def test_comparison_checks_order_identity():
    """`<` / `<=` should reject operands with different orders.

    Every other binary operation (`+`, `-`, `max`, `min`, `&`, `|`, `ite`)
    raises ValueError on an order mismatch; the comparison operators
    (`XADD::operator<` / `operator<=` in `src/XADD.cpp`) lack that check and
    silently compare, interpreting the operands' sylvan variables with the
    wrong order.
    """
    code, out, err = run_child("""
other_xa = D.XADD(D.VariableOrder())
f = make_leaf(xa, "1", "0")
g = make_leaf(other_xa, "2", "5")   # same shape, strictly bigger values
for op in ("<", "<="):
    try:
        result = f < g if op == "<" else f <= g
        raise AssertionError(
            f"mixed-order {op} returned {result!r} instead of raising"
        )
    except ValueError:
        pass
print("order check ok")
""")
    assert code == 0, f"subprocess failed with {code}:\n{err}"
    assert out.strip() == "order check ok"
