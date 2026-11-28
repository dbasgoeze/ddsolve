"""Tests for LinearComparison.__repr__ (exact strings for every
ComparisonType; repr shows the comparison as given, uncanonicalized)."""

import ddsolve


class TestLinearComparisonRepr:
    """Exact __repr__ strings for every ComparisonType.

    Format: `<normalized term> <op> 0`, matching `test_two_term_comparison`
    below (repr shows the comparison as given — no canonicalization of
    the operator, unlike `get_comparison`).
    """

    def _mk(self, products, ctype):
        return ddsolve.LinearComparison(
            ddsolve.LinearTerm([ddsolve.Product(v, c) for v, c in products]),
            ctype,
        )

    def test_equality(self):
        c = self._mk([(3, 1)], ddsolve.ComparisonType.kEquality)
        assert repr(c) == "1*x3 = 0"

    def test_inequality(self):
        c = self._mk([(3, 1)], ddsolve.ComparisonType.kInequality)
        assert repr(c) == "1*x3 != 0"

    def test_less_than(self):
        c = self._mk([(3, -1), (0, 1)], ddsolve.ComparisonType.kLessThan)
        assert repr(c) == "1 + -1*x3 < 0"

    def test_less_than_or_equal(self):
        c = self._mk([(3, 1)], ddsolve.ComparisonType.kLessThanOrEqual)
        assert repr(c) == "1*x3 <= 0"

    def test_greater_than(self):
        c = self._mk([(3, 1)], ddsolve.ComparisonType.kGreaterThan)
        assert repr(c) == "1*x3 > 0"

    def test_greater_than_or_equal(self):
        c = self._mk([(3, 1)], ddsolve.ComparisonType.kGreaterThanOrEqual)
        assert repr(c) == "1*x3 >= 0"

    def test_two_term_comparison(self):
        lhs = ddsolve.LinearTerm([ddsolve.Product(3, 1)])
        rhs = ddsolve.LinearTerm([ddsolve.Product(0, 5)])
        c = ddsolve.LinearComparison(lhs, rhs, ddsolve.ComparisonType.kLessThan)
        # A two-term comparison is normalized to `rhs - lhs < 0`.
        assert repr(c) == "-5 + 1*x3 < 0"
