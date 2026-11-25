"""Tests for the Product class:

`Product.__repr__()`, `Product.get_variable()`, `Product.get_coefficient()`.
"""

import ddsolve


class TestProductRepr:
    def test_constant_product(self):
        p = ddsolve.Product(0, 5)
        assert repr(p) == "5"

    def test_variable_product(self):
        p = ddsolve.Product(3, 2)
        assert repr(p) == "2*x3"

    def test_rational_coefficient(self):
        p = ddsolve.Product(1, "3/4")
        assert repr(p) == "3/4*x1"

    def test_string_constructor(self):
        p = ddsolve.Product(0, "2/3")
        assert repr(p) == "2/3"

    def test_negative_coefficient(self):
        p = ddsolve.Product(2, "-7")
        assert repr(p) == "-7*x2"

    def test_large_variable_id(self):
        p = ddsolve.Product(100, 1)
        assert repr(p) == "1*x100"


class TestProductGetVariable:
    def test_constant_var_id(self):
        p = ddsolve.Product(0, 5)
        assert p.get_variable() == 0

    def test_nonzero_var_id(self):
        p = ddsolve.Product(42, 3)
        assert p.get_variable() == 42

    def test_large_var_id(self):
        p = ddsolve.Product(999, 1)
        assert p.get_variable() == 999


class TestProductGetCoefficient:
    def test_integer_coefficient(self):
        p = ddsolve.Product(1, 7)
        assert p.get_coefficient() == "7"

    def test_rational_coefficient(self):
        p = ddsolve.Product(2, "5/3")
        assert p.get_coefficient() == "5/3"

    def test_negative_coefficient(self):
        p = ddsolve.Product(1, "-2/5")
        assert p.get_coefficient() == "-2/5"

    def test_one_coefficient(self):
        p = ddsolve.Product(3, 1)
        assert p.get_coefficient() == "1"

    def test_zero_coefficient(self):
        p = ddsolve.Product(1, 0)
        assert p.get_coefficient() == "0"
