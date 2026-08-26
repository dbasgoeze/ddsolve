Terms
=====

Term representations form the core of ddsolve's verification engine. They
encode probabilistic program expressions as mathematical objects that can be
manipulated by decision diagrams and SMT solvers.

Linear Terms
------------

Linear terms represent affine expressions over program variables. They are the
foundation for weakest precondition computations and constraint generation.

.. doxygenclass:: ddsolve::expression::LinearTerm
   :project: ddsolve
   :members:
   :undoc-members:

.. doxygenclass:: ddsolve::expression::LinearComparison
   :project: ddsolve
   :members:
   :undoc-members:

.. doxygenstruct:: ddsolve::expression::Product
   :project: ddsolve
   :members:
   :undoc-members:

.. doxygenenum:: ddsolve::expression::FinityType
   :project: ddsolve

SMT Terms
---------

SMT terms bridge the gap between decision diagrams and external SMT solvers.
They encode constraints in a format that solvers can process directly.

.. doxygenclass:: ddsolve::expression::SmtTerm
   :project: ddsolve
   :members:
   :undoc-members:

.. doxygenclass:: ddsolve::expression::SmtComparison
   :project: ddsolve
   :members:
   :undoc-members:

Comparison and Result Types
---------------------------

.. doxygenenum:: ddsolve::expression::ComparisonType
   :project: ddsolve

.. doxygenenum:: ddsolve::expression::SmtResult
   :project: ddsolve
