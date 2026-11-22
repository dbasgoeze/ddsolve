SMT API
=======

This page describes the high-level SMT API implemented in the core SMT
module (``smt/smt.cpp`` and ``smt/SmtTerm.cpp``). These functions orchestrate
SMT solver interaction for verification tasks: they build SMT expressions
from term comparisons, encode them with the selected :doc:`solver wrapper
<smt_integration>`, and translate solver results back into decision-diagram
values. The core entry points are ``LinearComparison::decideReal`` and
``LinearComparison::decideUInt``; their internal ``State`` class is templated
on the solver wrapper type and therefore not captured by Doxygen's XML
output. ``SmtTerm.cpp`` provides the concrete ``SmtTerm`` and ``SmtComparison``
method implementations directly on top of the selected SMT solver library.
For exact function signatures, refer directly to ``smt/smt.cpp`` and
``smt/SmtTerm.cpp``.
