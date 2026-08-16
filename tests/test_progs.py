import pytest
import ddsolve

def test_verifying_prog(verifier, verifying_prog):
    assert verifier[0].verify(verifying_prog[0])

def test_failing_prog(verifier, failing_prog):
    assert not verifier[0].verify(failing_prog[0])
