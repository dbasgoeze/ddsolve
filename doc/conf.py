"""Sphinx configuration for ddsolve documentation.

Generates documentation for both the C++ backend (via Doxygen XML + Breathe)
and the Python frontend (via autodoc from Google-style docstrings).

The native extension ddsolve._ddsolve is documented from its .pyi stub file
by loading it into sys.modules before autodoc imports the package.
"""

import sys
from pathlib import Path
import importlib.util

# ---------------------------------------------------------------------------
# Project metadata
# ---------------------------------------------------------------------------
project = "ddsolve"
copyright = "2025-2026, Daniel Basgöze"
html_title = "ddsolve Documentation"
html_last_updated_fmt = "%b %d, %Y"

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

with open(_stub_pyi) as f:
    _pyi_content = f.read()

# Replace C++ type annotations that are not valid Python types
_pyi_content = _pyi_content.replace(
    '"std::optional<unsigned int>"',
    '"Any"'
)

_pyi_content = "from __future__ import annotations\n" + _pyi_content

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
    "ddsolve": "../doxygen/xml",
}
breathe_default_project = "ddsolve"
breathe_default_members = ["members", "undoc-members"]
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
# Nitpicky mode – warn about all broken cross-references
# ---------------------------------------------------------------------------
nitpicky = True
nitpick_ignore = [
    ("cpp:type", "std::string"),
    ("cpp:type", "std::ostream"),
    ("cpp:type", "std::istream"),
    ("cpp:type", "std::function"),
    ("cpp:type", "std::shared_ptr"),
    ("cpp:type", "std::optional"),
    ("cpp:type", "std::variant"),
    ("cpp:type", "std::vector"),
    ("cpp:type", "std::pair"),
    ("cpp:type", "std::size_t"),
    ("cpp:type", "std::uint32_t"),
    ("cpp:type", "std::uint64_t"),
    ("cpp:type", "std::int32_t"),
    ("cpp:type", "std::int64_t"),
    ("cpp:type", "std::span"),
    ("cpp:type", "ddsolve::gsl::not_null"),
    ("cpp:type", "ddsolve::gsl::span"),
    ("cpp:type", "ddsolve::gsl::index"),
    ("cpp:type", "ddsolve::gsl::owner"),
    ("cpp:type", "ddsolve::smt::SmtWrapper"),
    ("cpp:type", "ddsolve::views::MergeView"),
    ("cpp:type", "ddsolve::views::MergeIterator"),
    ("cpp:type", "ddsolve::ArithmeticTypes"),
    ("cpp:type", "VariableId"),
    ("cpp:type", "sylvan"),
    ("cpp:type", "sylvan::Mtbdd"),
    ("cpp:type", "SylvanVariable"),
    ("cpp:type", "Z3Expression"),
    ("cpp:type", "cvc5"),
    ("cpp:type", "cvc5::Term"),
    ("cpp:type", "mpq_class"),
    ("cpp:type", "uint8_t"),
    ("py:class", "ddsolve._ddsolve"),
    ("py:class", "lark.visitors.Interpreter"),
    ("py:class", "lark.visitors.Transformer"),
    ("py:class", "lark.visitors.Visitor"),
]

# ---------------------------------------------------------------------------
# Suppress noisy warnings
# ---------------------------------------------------------------------------
suppress_warnings = [
    "exhaustivedocstring",
    "breathe.default_project",
]
