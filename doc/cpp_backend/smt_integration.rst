SMT Integration
===============

The SMT integration layer provides a unified interface for interacting with
external SMT solvers. It abstracts away solver-specific details behind a common
concept, allowing ddsolve to switch between Z3 and CVC5 at compile time.

SmtWrapper Concept
------------------

The ``SmtWrapper`` concept defines the interface that all SMT solver
implementations must provide. See ``SmtWrapper.hpp`` for the full definition.

.. doxygenconcept:: ddsolve::smt::SmtWrapper
   :project: ddsolve

Solver Implementations
----------------------

Z3 Wrapper
~~~~~~~~~~

The Z3 wrapper provides integration with the Z3 SMT solver.

.. doxygenclass:: ddsolve::smt::Z3Wrapper
   :project: ddsolve
   :members:
   :undoc-members:

CVC5 Wrapper
~~~~~~~~~~~~

The CVC5 wrapper provides integration with the CVC5 SMT solver.

.. doxygenclass:: ddsolve::smt::Cvc5Wrapper
   :project: ddsolve
   :members:
   :undoc-members:
