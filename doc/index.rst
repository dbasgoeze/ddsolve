ddsolve
=======

Overview
--------

ddsolve (DdSolve) is an expectation-based deductive verifier for probabilistic
programs. It verifies probabilistic programs by representing *quantitative
assertions* — real-valued generalizations of the predicates of classical
verification — as decision diagrams and discharging logical constraints to SMT
solvers. Its central data structure is the *typed extended decision diagram*
(TEDD), an extension of binary decision diagrams (BDDs) and algebraic decision
diagrams (ADDs) in which inner nodes are labeled by many-sorted first-order
formulae and terminal nodes by typed expressions.

.. note:: Naming

   Due to historical reasons, we name TEDDs **XDDs** in the code base and
   documentation. The C++ classes ``XDD``, ``XBDD``, and ``XADD`` implement
   TEDDs: ``XDD`` is the generic TEDD, ``XBDD`` is the Boolean-valued
   specialization (Bool-TEDD), and ``XADD`` is the real-valued specialization
   (EUReal-TEDD). Wherever this documentation says "XDD", you should read
   "TEDD".

A σ-TEDD for a type σ maps first-order interpretations to values of type σ.
Case expressions — the syntactic semantic basis of TEDDs — grow exponentially
in the number of conditionals and probabilistic choices of a program, in
particular when loops are unrolled. TEDDs capture the symmetries of these case
expressions and remain compact, and *theory-aware SMT-based pruning* removes
sub-branches that are unsatisfiable modulo the underlying theory (such as
linear arithmetic), keeping them small. These two ingredients are what make
deductive probabilistic program verification scale: ddsolve significantly
outperforms state-of-the-art wp-based verifiers such as Caesar, and scales to
programs with unbounded integers, arrays, conditioning (``observe``),
non-determinism, costs, and nonlinear expectations.

Key Features
------------

- **Typed extended decision diagrams (XDDs)**: Inner nodes labeled with
  many-sorted first-order formulae, terminal nodes labeled with typed
  expressions (e.g., over the extended non-negative reals :math:`\mathbb{R}^{\infty}_{\geq 0}`).
  With a fixed order on atomic formulae, reduced TEDDs are canonical
  representations of case expressions modulo propositional equivalence; classic
  BDDs and ADDs arise as special cases.
- **Theory-aware SMT-based pruning**: Paths whose conditions are unsatisfiable
  modulo the theory (e.g., mixed real/integer linear arithmetic LA, or its
  extension LA+ARR by N-indexed arrays) are pruned using SMT solvers. Pruning
  is essential for k-induction and accelerates verification by orders of
  magnitude.
- **Weakest pre-expectation calculus on diagrams**: A syntactic variant of the
  weakest pre-expectation (wp) calculus operates entirely on TEDDs, lifting
  the wp calculus — including conditioning, costs, and minimizing
  non-determinism — to decision diagrams. Proof rules for loops, both
  fixed-point iteration based and probabilistic k-induction, are lifted to
  TEDDs with SMT-checked quantitative entailments.
- **Multiple verification calculi**: The frontend supports weakest
  pre-expectation (wp), weakest liberal pre-expectation (wlp), conditional
  weakest pre-expectation (cwp), and expected runtime (ert), with both
  minimizing and maximizing non-determinism.
- **SMT integration**: Automatic generation and solving of first-order
  constraints with CVC5 or Z3, exploiting incremental SMT solving; pruning is
  modular and can be instantiated with different solvers.
- **Multithreaded diagram engine**: TEDDs are implemented on top of the
  efficient, thread-safe BDD library Sylvan, whose work-stealing framework
  Lace enables parallel TEDD operations with superlinear speedups in practice.
- **PGCL support**: Programs are written in the Probabilistic Guarded Command
  Language (pGCL) — imperative programs with probabilistic choice
  ``{C}[p]{C}``, pure non-deterministic choice ``{C}□{C}``, ``cost`` statements,
  ``observe`` statements, conditionals, and loops.
- **Dice support**: The frontend parses a large fragment of the Dice language
  for exact probabilistic inference.
- **Exact arithmetic**: Leaves and predicates can be represented generically
  (via CVC5 data structures) or as multivariate affine expressions with
  dedicated data structures; both support exact rational numbers.
- **Python interface**: The Python frontend provides a high-level API for
  defining programs, running verifiers, and inspecting results, together with
  a command-line interface.

Architecture
------------

ddsolve has a two-layer architecture:

**C++ Backend**

The C++ core implements the TEDD data structures and verification algorithms.
Key components include:

- TEDD classes (``XDD``, ``XBDD``, ``XADD``) forming a CRTP hierarchy, built on
  the Sylvan BDD library (with Lace-based multithreading)
- Core algorithms on TEDDs, all preserving the fixed order on formulae:
  ``APPLY`` (an n-ary generalization of Bryant's APPLY applying term operators
  such as ``ITE``, ``+``, or ``·``), ``SUBSTITUTE`` (variable-by-term
  substitution via ITEs), ``PRUNE`` (theory-aware pruning modulo a decidable
  theory), ``MINIMUM`` (pointwise minima of real-valued TEDDs for
  non-deterministic choice), and ``ENTAILS`` (checking quantitative
  entailments modulo a theory with an SMT solver)
- Term representations: generic SMT-solver expressions (CVC5) and dedicated
  multivariate affine (linear) expressions, both with exact rationals;
  expression normalization enables trivial pruning
- Variable ordering management: orderings based on occurrence in the program's
  AST, respecting the fixed linear order on atomic formulae required by
  canonicality
- SMT solver wrappers for constraint generation and checking (CVC5, Z3), using
  incremental solving
- Operation caching of TEDD node operations, and theory-agnostic syntactic
  equivalence checks on TEDDs (which, empirically, suffice to detect
  semantically identical TEDDs)

**Python Frontend**

The Python layer provides user-facing APIs:

- pGCL program parsing and pretty printing, resembling the frontend of Caesar
- Weakest pre-expectation style calculi (wp, wlp, cwp, ert) with minimizing and
  maximizing non-determinism, implemented on top of the backend's TEDD
  operations
- Loop reasoning via fixed-point iteration (with simultaneous loop unrolling
  for nested loops) and the k-induction principle
- Dice-based probabilistic program evaluation
- SMT-LIB parser for external constraint definitions
- Command-line interface for quick verification tasks

Approach in a Nutshell
----------------------

1. **Programs and expectations.** Programs are pGCL programs over many-sorted
   first-order theories (structures assign standard meanings to Booleans,
   naturals, integers, rationals, and extended unsigned reals). Expectations
   are random variables mapping program states to
   :math:`\mathbb{R}^{\infty}_{\geq 0}`; the weakest pre-expectation
   ``wp[C](X)`` maps every initial state to the minimal — under all
   resolutions of non-determinism — expected outcome of ``X`` upon
   termination, where ``observe`` statements enable conditioning on expected
   outcomes.
2. **Case expressions and TEDDs.** Expectations are represented syntactically
   as case expressions (``t ⇐ φ`` mapping formulae φ to typed terms t) and then
   as TEDDs: every root-to-terminal path conjoins (negated) node labels into
   one case. Reduced TEDDs are canonical modulo propositional equivalence.
3. **Operations on TEDDs.** wp is computed on TEDDs by lifting the classical
   rules to diagram operations (substitution for assignment, ITE for
   conditionals and observe, convex sums for probabilistic choice, pointwise
   minima for non-deterministic choice).
4. **Pruning.** After operations that may introduce unreachable branches,
   SMT-based pruning (modulo the chosen theory) deletes unsatisfiable
   sub-TEDDs.
5. **Loops.** Candidate upper bounds Y are verified or refuted via fixed-point
   iteration on the characteristic function (which corresponds to unrolling)
   or via probabilistic k-induction; both reduce to computing wp of the loop
   body and checking quantitative entailments with ENTAILS.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   cpp_backend
   python_frontend
