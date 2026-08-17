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
copyright = "2025, ddsolve contributors"

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
    "sphinx_autodoc_typehints",
]

# ---------------------------------------------------------------------------
# Breathe configuration – Doxygen XML output from C++ headers
# ---------------------------------------------------------------------------
breathe_projects = {
    "ddsolve": "../doxygen/xml",
}
breathe_default_project = "ddsolve"

# ---------------------------------------------------------------------------
# Napoleon configuration – Google-style docstring support
# ---------------------------------------------------------------------------
napoleon_google_docstring = True
napoleon_numpy_docstring = False

# ---------------------------------------------------------------------------
# Autodoc defaults
# ---------------------------------------------------------------------------
autodoc_default_options = {
    "members": True,
    "undoc-members": True,
    "show-inheritance": True,
}
