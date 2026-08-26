Python Frontend Overview
========================

The Python frontend provides a high-level interface for probabilistic program
verification. It is built on top of the C++ backend and exposes its capabilities
through Python modules.

.. automodule:: ddsolve
   :members:
   :undoc-members:
   :show-inheritance:

Modules
-------

The Python frontend is organized into the following modules:

- **dice** (`dice`) — Dice-based probabilistic program evaluation
- **pgcl** (`pgcl`) — PGCL parsing, pretty printing, and weakest precondition calculus
- **smt** (`smt`) — SMT-LIB parser for external constraint definitions
- **cli** (`api`) — Command-line interface
- **_ddsolve** (`cpp_bindings`) — Compiled C++ extension bindings
