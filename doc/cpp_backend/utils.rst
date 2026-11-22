.. _utils_8hpp:

Utilities
=========

Hash utilities, indirect references, and merge views for generic programming
support within the C++ backend. All of the types on this page live in the
header ``utils.hpp``.

Indirection Types
-----------------

.. doxygenstruct:: ddsolve::indirection
   :project: ddsolve
   :members:
   :undoc-members:

.. doxygenstruct:: ddsolve::indirection_less
   :project: ddsolve
   :members:
   :undoc-members:

.. doxygenstruct:: ddsolve::indirection_equal_to
   :project: ddsolve
   :members:
   :undoc-members:

Hash Functions
--------------

.. doxygenstruct:: ddsolve::IndirectHash
   :project: ddsolve

``IndirectHash`` hashes the value behind an indirect reference (pointer,
``std::unique_ptr``, …). Because Doxygen cannot express the C++20
``requires``-constrained overloads used to implement it, its member
functions are not rendered automatically: depending on whether ``T``
provides a ``hash()`` member function, ``operator()`` either calls that
member function on the dereferenced value or falls back to
``std::hash<T>``.

.. doxygenstruct:: ddsolve::MemberHash
   :project: ddsolve

.. doxygenstruct:: ddsolve::SpanHash
   :project: ddsolve
   :members:
   :undoc-members:

Hash Concepts
~~~~~~~~~~~~~

.. doxygenconcept:: ddsolve::Hashable
   :project: ddsolve

.. doxygenconcept:: ddsolve::MemberHashable
   :project: ddsolve

.. doxygenconcept:: ddsolve::StdHashable
   :project: ddsolve

Merge Views
-----------

.. doxygenclass:: ddsolve::views::MergeIterator
   :project: ddsolve
   :members:
   :undoc-members:

.. doxygenclass:: ddsolve::views::MergeView
   :project: ddsolve
   :members:
   :undoc-members:
