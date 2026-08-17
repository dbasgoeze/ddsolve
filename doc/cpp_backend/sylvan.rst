Sylvan Interface
================

The Sylvan interface provides the low-level interface to the
Sylvan decision diagram library. This is where the performance-critical
operations are implemented — it is not merely a thin binding.

Most functions in this layer are declared using C macros
(e.g. ``SylvanMTBDD_...``), which makes them difficult to
capture in Doxygen XML and therefore hard to document automatically.
The macros generate function declarations with varying signatures
depending on configuration flags, which further complicates
auto-documentation.

Rather than auto-generated bindings, this layer implements the
actual MTBDD/MTBDD-like operations (arithmetic, comparison, pruning,
etc.) that drive the decision diagram computations. The C header
(`SylvanInterface.h`) and C++ header (`SylvanInterface.hpp`) define
the public API surface, with the implementation in
`SylvanInterface.cpp` and `sylvan_ddsolve.c`.

Because of the macro-heavy declarations, this page provides a
conceptual overview rather than detailed per-function documentation.
For the exact function signatures, refer directly to the source
headers.
