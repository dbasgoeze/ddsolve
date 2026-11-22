"""Known-bug tests for C++ quirks of the Python-exposed XBDD interface.

These tests encode the *desired* behavior and are expected to FAIL with the
current C++ implementation.  They turn green once the underlying bug is
fixed — at that point the fix is confirmed and these assertions can be
relocated to the regular suite.

Quirks covered:

`XBDD * XADD` (`operator*`, `src/XBDD.hpp`) does not yield the selector
product: the result is not equal to the semantically identical
`bdd.ite(add, zero)`, and descending into the result with
`get_then()`/`get_else()` segfaults.

(Note: `XBDD.__eq__` comparing variable-order *identity* is intended
behavior — an order makes the diagrams different — and is covered as a
regular test in `tests/test_xbdd.py`.)

Each test runs its body in a subprocess: a failing assertion inside the
child would otherwise pin the traceback in the parent (keeping the C++
objects alive), tripping the Sylvan ref-count leak-guard in
`tests/conftest.py` and erroring every subsequent test.
"""

import subprocess
import sys

import pytest

CHILD_PREAMBLE = """\
import ddsolve

GT = ddsolve.ComparisonType.kGreaterThan


def term(*products):
    return ddsolve.LinearTerm([ddsolve.Product(v, c) for v, c in products])


def gt_one(var=3, k=1):
    # The comparison `x_var - k > 0` (non-trivially decidable).
    return ddsolve.LinearComparison(term((var, 1), (0, -k)), GT)


def make_node(order):
    return ddsolve.XBDD(order).obtain_repr(gt_one())
"""


def run_child(body):
    """Run a quirk check in a subprocess; return (returncode, stdout, stderr)."""
    result = subprocess.run(
        [sys.executable, "-c", CHILD_PREAMBLE + body],
        capture_output=True,
        text=True,
        timeout=60,
    )
    return result.returncode, result.stdout, result.stderr


class TestSelectorProduct:
    def test_product_equals_ite(self):
        code, out, err = run_child("""
order = ddsolve.VariableOrder()
node = make_node(order)
xadd = ddsolve.XADD(order)
one, zero = xadd.get_one(), xadd.get_zero()
assert node * one == node.ite(one, zero)
""")
        assert code == 0, f"subprocess failed with {code}:\n{err}"
        # BUG: `node * one != node.ite(one, zero)` although they encode the
        # same function.

    def test_product_children_reachable(self):
        # BUG: `get_then()` on a `bdd * add` result segfaults.  Once fixed,
        # the result is `ite(bdd, add, zero)`: then branch `1`, else `0`.
        code, out, err = run_child("""
order = ddsolve.VariableOrder()
node = make_node(order)
xadd = ddsolve.XADD(order)
one, zero = xadd.get_one(), xadd.get_zero()
product = node * one
then_ = product.get_then()
else_ = product.get_else()
assert then_.is_leaf() and else_.is_leaf()
assert then_.get_term() == one.get_term()
assert else_.get_term() == zero.get_term()
print("then=1 else=0")
""")
        assert code == 0, f"subprocess failed with {code}:\n{err}"
        assert out.strip() == "then=1 else=0"
