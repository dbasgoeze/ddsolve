ddsolve
===========

Overview
--------

ddsolve is an expectation-based probabilistic program verifier. It verifies
probabilistic programs by transforming them into decision diagram representations
and solving logical constraints using SMT solvers.

Probabilistic programs extend standard programs with random sampling and
observational conditioning. Verifying such programs requires reasoning about
distributions over execution traces rather than individual paths. ddsolve
addresses this by encoding program semantics into weighted decision diagrams
(XADDs and XBDDs) and using symbolic reasoning to compute verification
conditions.

Key Features
------------

- **Weighted decision diagrams**: Extended Arithmetic Decision Diagrams (XADDs)
  and Extended Binary Decision Diagrams (XBDDs) for compact representation of
  probabilistic distributions and logical predicates.
- **Multiple verification calculi**: Supports weakest precondition (wp), weakest
  liberal precondition (wlp), and Dice-based expectation evaluation.
- **SMT integration**: Automatic generation and solving of logical constraints
  using CVC5 or Z3.
- **PGCL support**: Programs are written in Probabilistic Guarded Command Language
  (PGCL), a well-studied formalism for probabilistic programs.
- **Python interface**: The Python frontend provides a high-level API for
  defining programs, running verifiers, and inspecting results.

Architecture
------------

ddsolve has a two-layer architecture:

**C++ Backend**

The C++ core implements the decision diagram data structures and verification
algorithms. Key components include:

- Decision diagram classes (``XDD``, ``XBDD``, ``XADD``) forming a CRTP hierarchy
- Term representations for linear arithmetic and SMT-based reasoning
- Variable ordering management for efficient diagram operations
- SMT solver wrappers for constraint generation and checking
- A Sylvan-based interface for high-performance decision diagram operations

**Python Frontend**

The Python layer provides user-facing APIs:

- PGCL program parsing and pretty printing
- Weakest precondition calculus implementation
- Dice-based probabilistic program evaluation
- SMT-LIB parser for external constraint definitions
- Command-line interface for quick verification tasks

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   cpp_backend
   python_frontend
