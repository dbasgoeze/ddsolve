import pytest
from pytest import fixture
import ddsolve
import gc
from pathlib import Path

def pytest_generate_tests(metafunc):

    name = "verifying_path"
    if name in metafunc.fixturenames:
        paths = Path("./verifying_progs/").glob("*.pgcl")
        metafunc.parametrize(name, paths, ids=lambda x: x.name)

    name = "failing_path"
    if name in metafunc.fixturenames:
        paths = Path("./failing_progs/").glob("*.pgcl")
        metafunc.parametrize(name, paths, ids=lambda x: x.name)


@fixture(autouse=True)
def sylvan_state():
    assert ddsolve.sylvan_count_refs() == 0
    yield
    gc.collect()
    assert ddsolve.sylvan_count_refs() == 0
    ddsolve.sylvan_gc()

@fixture
def variable_order(sylvan_state):
    rval = ddsolve.VariableOrder()
    rval = [rval]
    yield rval
    rval[0] = None

@fixture
def verifier(variable_order):
    rval = ddsolve.Verifier(variable_order[0])
    rval = [rval]
    yield rval
    rval[0] = None


def _prog(path, order):
    with open(path) as f:
        rval = f.read()
    rval = ddsolve.parse(rval, order)
    return rval


@fixture
def verifying_prog(verifying_path, variable_order):
    rval = _prog(verifying_path, variable_order[0])
    rval = [rval]
    yield rval
    rval[0] = None


@fixture
def failing_prog(failing_path, variable_order):
    rval = _prog(failing_path, variable_order[0])
    rval = [rval]
    yield rval
    rval[0] = None
