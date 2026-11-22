"""Sphinx configuration for ddsolve documentation.

Generates documentation for both the C++ backend (via Doxygen XML + Breathe)
and the Python frontend (via autodoc from Google-style docstrings).

The native extension ddsolve._ddsolve is documented from its .pyi stub file
by loading it into sys.modules before autodoc imports the package.
"""

import sys
import tomllib
import warnings
from pathlib import Path

# Breathe 5.1.0 does not know the "conceptparts" element that newer Doxygen
# versions emit for concepts. It skips the element after warning; silence
# that known-noise ParseWarning.
warnings.filterwarnings(
    "ignore",
    message=r'.*unexpected element "conceptparts".*',
)


# ---------------------------------------------------------------------------
# Project metadata
# ---------------------------------------------------------------------------
project = "ddsolve"
copyright = "2025-2026, Daniel Basgöze"
html_title = "ddsolve Documentation"
html_last_updated_fmt = "%b %d, %Y"

# Take the version from the project metadata in pyproject.toml so that it
# stays in sync with the installed package.
_pyproject = Path(__file__).parent.parent / "pyproject.toml"
with open(_pyproject, "rb") as _f:
    _project_meta = tomllib.load(_f)
version = release = _project_meta["project"]["version"]

# ---------------------------------------------------------------------------
# Stub loading – inject ddsolve._ddsolve from .pyi before any imports
#
# ddsolve/__init__.py does "from ddsolve._ddsolve import *", which requires
# the _ddsolve module to exist. Since we don't compile the native extension
# for docs, we load the .pyi stub file and register it in sys.modules.
#
# We prepend "from __future__ import annotations" to handle forward
# references in the stub (e.g., LinearComparison references LinearTerm
# before it is defined).
# ---------------------------------------------------------------------------
_stub_pyi = Path(__file__).parent.parent / "src" / "ddsolve" / "_ddsolve.pyi"

if not _stub_pyi.exists():
    sys.exit(
        f"ERROR: {_stub_pyi} not found.\n"
        "The documentation autodocs the compiled C++ extension from its "
        ".pyi type stub. Make sure you run sphinx-build from a full "
        "checkout of the ddsolve repository."
    )

with open(_stub_pyi) as f:
    _pyi_content = f.read()

# Provide typing.Any in the exec namespace so the replaced "Any" annotation
# below resolves.
_pyi_header = "from __future__ import annotations\nfrom typing import Any\n"

# Replace C++ type annotations that are not valid Python types
_pyi_content = _pyi_content.replace(
    '"std::optional<unsigned int>"',
    '"Any"'
)

_pyi_content = _pyi_header + _pyi_content

_stub_mod = type(sys)("ddsolve._ddsolve")
_stub_mod.__file__ = str(_stub_pyi)
_stub_mod.__loader__ = None
_stub_mod.__package__ = "ddsolve"
exec(_pyi_content, _stub_mod.__dict__)
sys.modules["ddsolve._ddsolve"] = _stub_mod

# ---------------------------------------------------------------------------
# Theme
# ---------------------------------------------------------------------------
html_theme = "furo"

# Never pick up stray files from the build output or the virtual environment
# that the Makefile creates inside doc/.
exclude_patterns = ["_build", ".venv"]


# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
# Add the src/ directory so autodoc can find the ddsolve package
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

# ---------------------------------------------------------------------------
# Extensions
# ---------------------------------------------------------------------------
extensions = [
    "breathe",
    "sphinx.ext.autodoc",
    "sphinx.ext.napoleon",
    "sphinx.ext.viewcode",
    "sphinx.ext.intersphinx",
    "sphinx_autodoc_typehints",
]

# ---------------------------------------------------------------------------
# Intersphinx – external documentation links
# ---------------------------------------------------------------------------
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
}

# ---------------------------------------------------------------------------
# Breathe configuration – Doxygen XML output from C++ headers
# ---------------------------------------------------------------------------
breathe_projects = {
    "ddsolve": "_build/doxygen/xml",
}
breathe_default_project = "ddsolve"
# Note: breathe_default_members is intentionally NOT set. Directives that
# want members list them explicitly; leaving it unset keeps small utility
# classes (e.g. IndirectHash, whose constrained overloads Doxygen cannot
# represent) from producing duplicate C++ declarations.
breathe_domain_by_extension = {
    ".hpp": "cpp",
    ".h": "cpp",
    ".cpp": "cpp",
}

# ---------------------------------------------------------------------------
# Napoleon configuration – Google-style docstring support
# ---------------------------------------------------------------------------
napoleon_google_docstring = True
napoleon_numpy_docstring = False
napoleon_include_init_with_doc = True

# ---------------------------------------------------------------------------
# Autodoc defaults
# ---------------------------------------------------------------------------
autodoc_default_options = {
    "members": True,
    "undoc-members": True,
    "show-inheritance": True,
}

# ---------------------------------------------------------------------------
# Autodoc type hints
# ---------------------------------------------------------------------------
autodoc_typehints = "description"
autodoc_typehints_description_target = "documented"

# ---------------------------------------------------------------------------
# Nitpicky mode – warn about all broken cross-references.
#
# Everything resolvable is documented on a doc page, so this list only
# contains types we genuinely cannot document from the ddsolve sources:
#   * C++ standard library types and fixed-width integer types
#   * GMP's mpq_class
#   * The cvc5 and Sylvan (sylvan::Mtbdd) third-party library types
#   * Python classes that autodoc cannot resolve (lark base classes)
# Note: Sphinx emits unresolved C++ references with the node type
# "cpp:identifier" (not "cpp:type"), so ignore entries must use that type.
nitpicky = True
nitpick_ignore = [
    ("cpp:identifier", "std::string"),
    ("cpp:identifier", "std::ostream"),
    ("cpp:identifier", "std::istream"),
    ("cpp:identifier", "std::function"),
    ("cpp:identifier", "std::shared_ptr"),
    ("cpp:identifier", "std::optional"),
    ("cpp:identifier", "std::variant"),
    ("cpp:identifier", "std::vector"),
    ("cpp:identifier", "std::pair"),
    ("cpp:identifier", "std::size_t"),
    ("cpp:identifier", "std::uint32_t"),
    ("cpp:identifier", "std::uint64_t"),
    ("cpp:identifier", "std::int32_t"),
    ("cpp:identifier", "std::int64_t"),
    ("cpp:identifier", "std::span"),
    ("cpp:identifier", "size_t"),
    ("cpp:identifier", "uint8_t"),
    ("cpp:identifier", "uint32_t"),
    ("cpp:identifier", "uint64_t"),
    ("cpp:identifier", "sylvan"),
    ("cpp:identifier", "sylvan::Mtbdd"),
    ("cpp:identifier", "cvc5"),
    ("cpp:identifier", "cvc5::Term"),
    ("cpp:identifier", "mpq_class"),
    ("cpp:identifier", "VariableOrder<Term>::VariableOrderPointer"),
    ("cpp:identifier", "VariableOrder<T>::VariableOrderPointer"),
    # Breathe renders namespace path components of declarations (e.g. the
    # "ddsolve::expression" in "ddsolve::expression::LinearTerm") as
    # references. Only one page may declare a namespace hierarchy (multiple
    # doxygenfile/doxygennamespace renders duplicate each other), so these
    # qualifier references cannot be targeted everywhere.
    ("cpp:identifier", "ddsolve::expression"),
    ("cpp:identifier", "expression"),
    # External solver/Sylvan library types that ddsolve cannot document.
    ("cpp:identifier", "z3"),
    ("cpp:identifier", "z3::expr"),
    # Rendering a qualified alias (e.g. "using ddsolve::smt::Z3Expression =
    # std::optional<z3::expr>") makes Breathe emit a stray reference to the
    # namespace itself, which the C++ domain cannot resolve.
    ("cpp:identifier", "ddsolve::smt"),
    ("py:class", "ddsolve._ddsolve"),
    ("py:class", "lark.visitors.Interpreter"),
    ("py:class", "lark.visitors.Transformer"),
    ("py:class", "lark.visitors.Visitor"),
]

# ---------------------------------------------------------------------------
# Suppress noisy warnings
# ---------------------------------------------------------------------------
suppress_warnings = [
    "breathe.default_project",
]


# ---------------------------------------------------------------------------
# Doxygen include-graph targets
#
# Breathe renders the `#include <...>` line of every class/struct/concept as
# a reference to the Doxygen *file compound* id of the included header (e.g.
# "SmtWrapper_8hpp"). Such a target is normally defined by rendering the
# file itself with `doxygenfile`, but that also re-declares every namespace
# it contains — and Sphinx may declare a namespace only once, so this is not
# possible on all pages. Docutils lower-cases explicit RST targets, so the
# exact-case Doxygen id cannot be written in RST either. We therefore inject
# the missing targets programmatically, pointing them at the current page.
_FILE_COMPOUND_TARGETS = {
    "cpp_backend/smt_integration": ["SmtWrapper_8hpp"],
}


def _inject_doxygen_file_targets(app, doctree):
    """Register exact-case Doxygen file-compound ids as RST targets."""
    from docutils import nodes as docutils_nodes

    docname = app.env.current_document.docname
    std_domain = app.env.domains["std"]
    for refid in _FILE_COMPOUND_TARGETS.get(docname, []):
        # Anchor node so the rendered reference can jump somewhere.
        target = docutils_nodes.target(ids=[refid], names=[refid])
        doctree.note_explicit_target(target)
        doctree.insert(0, target)
        # The std domain collects explicit targets before this hook runs
        # (its doctree-read handler is connected earlier), so register the
        # label directly. Breathe emits `refexplicit` references, which
        # resolve against anonlabels only.
        std_domain.anonlabels[refid] = (docname, refid)


def setup(app):
    """Add the event hook for the injected targets (called by Sphinx)."""
    app.connect("doctree-read", _inject_doxygen_file_targets)
