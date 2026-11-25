"""Tests for environment-variable paths and module-level APIs.

The env vars SYLVAN_MAXMEM, SYLVAN_INITIAL_RATIO, SYLVAN_TABLE_RATIO,
and LACE_WORKERS are read once when the extension module is first
imported.  The "env var set" and "invalid integer" code paths cannot be
exercised from a test inside the same process, so we test them via
subprocess.

Also tests: sylvan_gc(), sylvan_count_refs(), LaceManager, PrintModelsManager.
"""

import os
import subprocess
import sys

import ddsolve


class TestSylvanGc:
    def test_sylvan_gc_noop(self):
        """sylvan_gc() should not raise."""
        ddsolve.sylvan_gc()

    def test_sylvan_count_refs(self):
        refs = ddsolve.sylvan_count_refs()
        assert isinstance(refs, int)
        assert refs >= 0


class TestLaceManager:
    """LaceManager context manager — enter/exit path."""

    def test_context_manager(self):
        with ddsolve.LaceManager():
            pass  # Should acquire/release Lace thread

    def test_nested_context_managers(self):
        with ddsolve.LaceManager():
            with ddsolve.LaceManager():
                pass  # Nested LaceManager

    def test_multiple_exits(self):
        for _ in range(3):
            with ddsolve.LaceManager():
                pass


class TestPrintModelsManager:
    """PrintModelsManager context manager — enter/exit path."""

    def test_context_manager(self):
        with ddsolve.PrintModelsManager():
            pass  # Should enable/disable model printing

    def test_nested_context_managers(self):
        with ddsolve.PrintModelsManager():
            with ddsolve.PrintModelsManager():
                pass

    def test_multiple_exits(self):
        for _ in range(3):
            with ddsolve.PrintModelsManager():
                pass


class TestEnvVarsSubprocess:
    """Test env-var paths via subprocess (module init reads them)."""

    def _run_with_env(self, env_vars, code):
        """Run a Python snippet with given env vars and return stdout."""
        env = dict(os.environ)
        for k, v in env_vars.items():
            env[k] = v
        result = subprocess.run(
            [sys.executable, "-c", code],
            capture_output=True, text=True, timeout=120, env=env,
        )
        return result

    def test_sylvan_maxmem(self):
        """SYLVAN_MAXMEM=1000000000 should not crash module init."""
        result = self._run_with_env(
            {"SYLVAN_MAXMEM": "1000000000"},
            "import ddsolve; print('ok')",
        )
        assert result.returncode == 0
        assert "ok" in result.stdout

    def test_sylvan_maxmem_large(self):
        """SYLVAN_MAXMEM=999999999 should not crash."""
        result = self._run_with_env(
            {"SYLVAN_MAXMEM": "999999999"},
            "import ddsolve; print('ok')",
        )
        assert result.returncode == 0

    def test_sylvan_initial_ratio(self):
        """SYLVAN_INITIAL_RATIO=5 should not crash."""
        result = self._run_with_env(
            {"SYLVAN_INITIAL_RATIO": "5"},
            "import ddsolve; print('ok')",
        )
        assert result.returncode == 0

    def test_sylvan_table_ratio(self):
        """SYLVAN_TABLE_RATIO=3 should not crash."""
        result = self._run_with_env(
            {"SYLVAN_TABLE_RATIO": "3"},
            "import ddsolve; print('ok')",
        )
        assert result.returncode == 0

    def test_lace_workers(self):
        """LACE_WORKERS=2 should not crash."""
        result = self._run_with_env(
            {"LACE_WORKERS": "2"},
            "import ddsolve; print('ok')",
        )
        assert result.returncode == 0

    def test_all_env_vars(self):
        """Set all env vars simultaneously."""
        result = self._run_with_env(
            {
                "SYLVAN_MAXMEM": "2000000000",
                "SYLVAN_INITIAL_RATIO": "1",
                "SYLVAN_TABLE_RATIO": "2",
                "LACE_WORKERS": "1",
            },
            "import ddsolve; print('ok')",
        )
        assert result.returncode == 0

    def test_invalid_integer_uses_default(self):
        """SYLVAN_MAXMEM=not-a-number should use the default (no crash)."""
        result = self._run_with_env(
            {"SYLVAN_MAXMEM": "not-a-number"},
            "import ddsolve; print('ok')",
        )
        assert result.returncode == 0
        assert "ok" in result.stdout

    def test_invalid_initial_ratio_uses_default(self):
        """SYLVAN_INITIAL_RATIO=xyz should use the default."""
        result = self._run_with_env(
            {"SYLVAN_INITIAL_RATIO": "xyz"},
            "import ddsolve; print('ok')",
        )
        assert result.returncode == 0

    def test_empty_string_uses_default(self):
        """SYLVAN_MAXMEM='' should use the default."""
        result = self._run_with_env(
            {"SYLVAN_MAXMEM": ""},
            "import ddsolve; print('ok')",
        )
        assert result.returncode == 0

    def test_negative_env_var(self):
        """Negative SYLVAN_MAXMEM causes Sylvan to fail (non-zero exit)."""
        result = self._run_with_env(
            {"SYLVAN_MAXMEM": "-1"},
            "import ddsolve; print('ok')",
        )
        # Negative value causes "Cannot allocate memory" — documented
        # as current behaviour, not a bug to fix.
        assert result.returncode != 0

    def test_lace_workers_invalid(self):
        """LACE_WORKERS=abc should use the default."""
        result = self._run_with_env(
            {"LACE_WORKERS": "abc"},
            "import ddsolve; print('ok')",
        )
        assert result.returncode == 0

    def test_trailing_garbage_uses_default(self):
        """SYLVAN_MAXMEM=999abc triggers from_chars failure path.

        `std::from_chars` parses digits until 'a', then `ptr != end` so
        `error.ec != std::errc{}` — the `from_chars` failure branch at
        `getIntegerEnvironmentVariable`.
        """
        result = self._run_with_env(
            {"SYLVAN_MAXMEM": "999abc"},
            "import ddsolve; print('ok')",
        )
        assert result.returncode == 0
        assert "ok" in result.stdout
