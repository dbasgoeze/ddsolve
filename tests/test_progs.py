import pytest
import ddsolve

def test_verifying_prog(verifier, verifying_prog):
    result = verifier[0].verify(verifying_prog[0])
    assert result is True, f"expected verified, got {result!r}"

def test_failing_prog(verifier, failing_prog):
    result = verifier[0].verify(failing_prog[0])
    assert result is False, f"expected refuted, got {result!r}"
