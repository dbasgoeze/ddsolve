"""Tests for the LinearTerm value object: repr, scale, and iteration."""

import ddsolve


class TestLinearTermRepr:
    def test_zero_term(self):
        t = ddsolve.LinearTerm([])
        assert repr(t) == "0"

    def test_single_term(self):
        t = ddsolve.LinearTerm([ddsolve.Product(3, 2)])
        assert repr(t) == "2*x3"

    def test_multi_term(self):
        t = ddsolve.LinearTerm([
            ddsolve.Product(0, 1),
            ddsolve.Product(3, -1),
        ])
        assert repr(t) == "1 + -1*x3"

    def test_rational_coefficients(self):
        t = ddsolve.LinearTerm([
            ddsolve.Product(1, "3/4"),
            ddsolve.Product(2, "1/2"),
        ])
        assert repr(t) == "3/4*x1 + 1/2*x2"

    def test_infinite_term(self):
        t = ddsolve.LinearTerm(ddsolve.FinityType.kPositiveInfinite)
        assert repr(t) == "∞"

    def test_neg_infinite_term(self):
        t = ddsolve.LinearTerm(ddsolve.FinityType.kNegativeInfinite)
        assert repr(t) == "-∞"

    def test_iteration(self):
        """LinearTerm.__iter__ yields the Products in term order."""
        t = ddsolve.LinearTerm([
            ddsolve.Product(1, 3),
            ddsolve.Product(2, "1/2"),
        ])
        products = list(t)
        assert len(products) == 2
        assert products[0].get_variable() == 1
        assert products[1].get_variable() == 2


class TestLinearTermScale:
    def test_scale_by_integer(self):
        t = ddsolve.LinearTerm([ddsolve.Product(3, 2)])
        scaled = t.scale("3")
        assert repr(scaled) == "6*x3"

    def test_scale_by_rational(self):
        t = ddsolve.LinearTerm([ddsolve.Product(1, "2/3")])
        scaled = t.scale("3/2")
        assert repr(scaled) == "1*x1"

    def test_scale_zero(self):
        t = ddsolve.LinearTerm([ddsolve.Product(3, 2), ddsolve.Product(0, 1)])
        scaled = t.scale("0")
        assert repr(scaled) == "0"

    def test_scale_negative(self):
        t = ddsolve.LinearTerm([ddsolve.Product(1, 5)])
        scaled = t.scale("-1")
        assert repr(scaled) == "-5*x1"

    def test_scale_infinite(self):
        t = ddsolve.LinearTerm(ddsolve.FinityType.kPositiveInfinite)
        scaled = t.scale("2")
        # Infinite terms are unchanged by scaling
        assert repr(scaled) == "∞"

    def test_scale_copy_constructed(self):
        t1 = ddsolve.LinearTerm([ddsolve.Product(1, 3)])
        t2 = ddsolve.LinearTerm(t1)  # copy constructor
        scaled = t2.scale("2")
        assert repr(scaled) == "6*x1"
