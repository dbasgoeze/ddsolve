"""ddsolve - Expectation-Based Probabilistic Program Verifier.

This package provides tools for verifying probabilistic programs using
weighted program logics and decision diagram representations (XBDD/XADD).
"""

from ddsolve._ddsolve import *
from ddsolve.parser import parse
from ddsolve.wp import Verifier

import ddsolve.smt_parser
