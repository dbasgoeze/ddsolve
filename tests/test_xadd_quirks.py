"""Known-bug tests for C++ quirks of the Python-exposed XADD interface.

These tests encode the *desired* behavior and are expected to FAIL with the
current C++ implementation.  They turn green once the underlying bug is
fixed — at that point the fix is confirmed and these assertions can be
relocated to the regular suite.

Quirks covered:

1. `XADD.get_neg_infty` (binding in `src/main.cpp`) constructs its term with
   `FinityType::kPositiveInfinite`, the same constant as `get_infty`, so it
   returns positive infinity instead of negative infinity.

2. `XADD.min` / `XADD.max` lose information when both operands are leaf
   diagrams carrying *distinct* linear terms.  Per Algorithm 4 (two-terminal
   case, l. 5–6) they must case-split on `Obtain(L_T(X1) < L_T(X2), X1, X2)`,
   producing a diagram with the crossing condition as the top variable;
   instead both return a single leaf (one operand's term), so the other
   operand is silently discarded.

The test bodies run in subprocesses: a failing assertion inside the child
would otherwise pin the traceback in the parent (keeping the C++ objects
alive), tripping the Sylvan ref-count leak-guard in `tests/conftest.py`.
"""

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
    return result.returncode, result.stdout, result.stderr


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

    Expected (Algorithm 4, two-terminal case): case-split on
    `Obtain(L_T(X1) < L_T(X2), X1, X2)` instead of collapsing to one leaf.
    """

    def test_min_case_splits(self):
        code, out, err = run_child("""
minimum = A.min(B)
assert not minimum.is_leaf(), f"min collapsed to leaf: {minimum.get_term()!r}"
assert minimum == cond.ite(A, B), "min is not ite(A < B, A, B)"
assert minimum.get_then().get_term() == A.get_term()
assert minimum.get_else().get_term() == B.get_term()
print("min ok")
""")
        assert code == 0, f"subprocess failed with {code}:\n{err}"
        assert out.strip() == "min ok"

    def test_max_case_splits(self):
        code, out, err = run_child("""
maximum = A.max(B)
assert not maximum.is_leaf(), f"max collapsed to leaf: {maximum.get_term()!r}"
assert maximum == cond.ite(B, A), "max is not ite(A < B, B, A)"
assert maximum.get_then().get_term() == B.get_term()
assert maximum.get_else().get_term() == A.get_term()
print("max ok")
""")
        assert code == 0, f"subprocess failed with {code}:\n{err}"
        assert out.strip() == "max ok"


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
