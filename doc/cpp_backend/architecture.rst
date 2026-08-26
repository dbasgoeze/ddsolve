Architecture
============

Namespace Overview
------------------

The C++ backend is organized into several namespaces that reflect the logical
structure of the verification engine:

``ddsolve``
    The root namespace containing top-level types and utilities. This includes
    the core decision diagram classes, metric measures, and general-purpose
    utility structures.

``ddsolve::expression``
    Term representations used in verification. Contains linear arithmetic terms
    (``LinearTerm``, ``LinearComparison``), SMT-based terms (``SmtTerm``,
    ``SmtComparison``), and related types (``ComparisonType``, ``SmtResult``,
    ``Product``, ``FinityType``).

``ddsolve::smt``
    SMT solver abstraction layer. Defines the ``SmtWrapper`` concept and
    concrete implementations for Z3 and CVC5 solvers.

``ddsolve::views``
    Range-view utilities for iterating over decision diagram nodes. Contains
    ``MergeView`` and ``MergeIterator`` for combining multiple ranges.

``gsl``
    Guidelines Support Library types for non-owning references and contract
    assertions (``not_null``, ``span``, ``index``, ``owner``).

Module Map
----------

The C++ source files are organized into the following modules:

Decision Diagrams
    ``XDD.hpp``, ``XBDD.hpp``, ``XADD.hpp``, ``XADD.cpp`` — Core decision
    diagram implementations with CRTP hierarchy.

Terms
    ``LinearTerm.hpp``, ``SmtTerm.hpp``, ``ArithmeticTypes.hpp``,
    ``MetricMeasure.hpp`` — Term representations and arithmetic types.

SMT Integration
    ``smt/SmtWrapper.hpp``, ``smt/Z3Wrapper.hpp``, ``smt/Cvc5Wrapper.hpp``,
    ``smt/smt.cpp``, ``smt/SmtTerm.cpp`` — SMT solver interface and term
    management.

Variable Management
    ``VariableOrder.hpp`` — Variable ordering and substitution mechanisms.

Utilities
    ``utils.hpp`` — Hash functions, indirect references, merge views.

Sylvan Interface
    ``sylvan/SylvanInterface.hpp``, ``sylvan/SylvanInterface.cpp``,
    ``sylvan/SylvanInterface.h``, ``sylvan/sylvan_ddsolve.c``,
    ``sylvan/sylvan_ddsolve.h`` — Low-level Sylvan integration for
    performance-critical operations.
