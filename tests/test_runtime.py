"""Tests for module-level runtime facilities: sylvan_gc/sylvan_count_refs,
LaceManager, and PrintModelsManager."""

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
    """LaceManager context manager — enter/exit path.

    `__enter__` resumes Lace (acquiring a thread), `__exit__` suspends it.
    The acquired C++ state is not observable from Python, so the tests pin
    the context-manager protocol: both hooks must succeed and return
    `None`, and nesting / repetition must compose without error.
    """

    def test_context_manager(self):
        manager = ddsolve.LaceManager()
        assert manager.__enter__() is None
        assert manager.__exit__(None, None, None) is None

    def test_nested_context_managers(self):
        outer, inner = ddsolve.LaceManager(), ddsolve.LaceManager()
        assert outer.__enter__() is None
        assert inner.__enter__() is None
        # Suspend in reverse (nesting) order.
        assert inner.__exit__(None, None, None) is None
        assert outer.__exit__(None, None, None) is None

    def test_multiple_exits(self):
        for _ in range(3):
            manager = ddsolve.LaceManager()
            assert manager.__enter__() is None
            assert manager.__exit__(None, None, None) is None


class TestPrintModelsManager:
    """PrintModelsManager context manager — enter/exit path.

    `__enter__` enables solver model printing, `__exit__` disables it.
    The print flag lives in the C++ layer and is not observable from
    Python, so the tests pin the context-manager protocol: both hooks
    must succeed and return `None`, and nesting / repetition must
    compose without error.
    """

    def test_context_manager(self):
        manager = ddsolve.PrintModelsManager()
        assert manager.__enter__() is None
        assert manager.__exit__(None, None, None) is None

    def test_nested_context_managers(self):
        outer, inner = ddsolve.PrintModelsManager(), ddsolve.PrintModelsManager()
        assert outer.__enter__() is None
        assert inner.__enter__() is None
        assert inner.__exit__(None, None, None) is None
        assert outer.__exit__(None, None, None) is None

    def test_multiple_exits(self):
        for _ in range(3):
            manager = ddsolve.PrintModelsManager()
            assert manager.__enter__() is None
            assert manager.__exit__(None, None, None) is None
