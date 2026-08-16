from lark import Transformer, Discard, v_args, Tree, Lark
from lark.visitors import Interpreter
from tqdm.auto import tqdm
from itertools import count

import ddsolve._ddsolve as ddsolve

from ddsolve.pprint import fpprint

smt_pgcl_parser = Lark.open(
    "smt_pgcl.lark",
    rel_to=__file__,
    propagate_positions=True,
    maybe_placeholders=True,
    parser="lalr",
)


class ToDDSolveTypes(Transformer):
    def __init__(self, order, prog):
        self.prog = prog
        self.xbdd = ddsolve.SmtXBDD(order)
        self.xadd = ddsolve.SmtXADD(order)
        self.to_comparison_type = {
            "==": ddsolve.ComparisonType.kEquality,
            ">": ddsolve.ComparisonType.kGreaterThan,
            ">=": ddsolve.ComparisonType.kGreaterThanOrEqual,
            "!=": ddsolve.ComparisonType.kInequality,
            "<": ddsolve.ComparisonType.kLessThan,
            "<=": ddsolve.ComparisonType.kLessThanOrEqual,
        }

    def gatherVars(self, tree):
        self.variables = dict()
        for v in tree.find_data("variable_decl"):
            id = v.children[1].value
            match v.children[0]:
                case "int":
                    tmp = ddsolve.SmtTerm.mk_int_var(id)
                case "real":
                    tmp = ddsolve.SmtTerm.mk_real_var(id)

            if tmp in self.variables.keys():
                raise RuntimeError(f"Multple declarations for the same variable: {id}")
            self.variables[id] = tmp

    def gatherFuncs(self, tree):
        self.funcs = dict()
        for v in tree.find_data("function_decl"):
            id = v.children[1].value
            match v.children[0]:
                case "int":
                    tmp = ddsolve.SmtTerm.mk_int_func(id, v.children[2])
                case "real":
                    tmp = ddsolve.SmtTerm.mk_real_func(id, v.children[2])

            if tmp in self.funcs.keys():
                raise RuntimeError(f"Multple declarations for the same function: {id}")
            self.funcs[id] = tmp

    def gatherArrays(self, tree):
        self.arrays = dict()
        for v in tree.find_data("array_decl"):
            id = v.children[1].value
            match v.children[0]:
                case "int":
                    tmp = ddsolve.SmtTerm.mk_int_array(id)
                case "real":
                    tmp = ddsolve.SmtTerm.mk_real_array(id)

            if tmp in self.funcs.keys():
                raise RuntimeError(f"Multple declarations for the same array: {id}")
            self.arrays[id] = tmp

    def transform(self, tree):
        self.gatherVars(tree)
        self.gatherFuncs(tree)
        self.gatherArrays(tree)
        return super().transform(tree)

    def variable_decl(self, tree):
        return Discard

    def function_decl(self, tree):
        return Discard

    def array_decl(self, tree):
        return Discard

    def term_array(self, tree):
        id = tree[0]
        return self.arrays[id]

    def term_array_select(self, tree):
        [a, idx] = tree
        return a.array_select(idx)

    def term_variable(self, tree):
        id = tree[0]
        return self.variables[id]

    def term_function(self, tree):
        id = tree[0]
        return self.funcs[id]

    def term_function_call(self, tree):
        return tree[0].apply_func(tree[1:])

    def term_int(self, tree):
        val = tree[0]
        return ddsolve.SmtTerm.mk_int(val)

    def term_real(self, tree):
        val = tree[0] + "/" + tree[1]
        return ddsolve.SmtTerm.mk_real(val)

    def term_add(self, tree):
        a = tree[0]
        b = tree[1]
        return a + b

    def term_mul(self, tree):
        a = tree[0]
        b = tree[1]
        return a * b

    def term_div(self, tree):
        a = tree[0]
        b = tree[1]
        return a / b

    def term_sub(self, tree):
        a = tree[0]
        b = tree[1]
        return a - b

    def term_to_real(self, tree):
        a = tree[0]
        return a.to_real()

    def term_to_int(self, tree):
        a = tree[0]
        return a.to_int()

    def expectation_term(self, tree):
        a = tree[0]
        return self.xadd.get_term(a)

    def comparison_base(self, tree):
        a = tree[0]
        op = tree[1]
        b = tree[2]
        return ddsolve.SmtComparison(a, b, self.to_comparison_type[op])

    def bool_comparison(self, tree):
        a = tree[0]
        return self.xbdd.obtain_repr(a)

    def bool_and(self, tree):
        a = tree[0]
        b = tree[1]
        return a & b

    def bool_or(self, tree):
        a = tree[0]
        b = tree[1]
        return a | b

    def bool_negation(self, tree):
        a = tree[0]
        return ~a

    def bool_ite(self, tree):
        a = tree[0]
        b = tree[1]
        c = tree[2]
        return a.ite(b, c)

    def bool_finite_forall(self, tree):
        [var, low, high, expr] = tree
        rval = self.xbdd.get_true()
        for i in range(int(low), int(high) + 1):
            rval = rval & expr.substitute(var, ddsolve.SmtTerm.mk_int(str(i)))
        print(rval)
        return rval

    def comparison_and(self, tree):
        [lhs, rhs] = tree
        return lhs & rhs

    def comparison_or(self, tree):
        [lhs, rhs] = tree
        return lhs & rhs

    def comparison_negation(self, tree):
        a = tree[0]
        return ~a

    def comparison_finite_forall(self, tree):
        [var, low, high, expr] = tree

        rval = expr.substitute(var, ddsolve.SmtTerm.mk_int(str(low)))
        for i in range(int(low) + 1, int(high) + 1):
            rval = rval & expr.substitute(var, ddsolve.SmtTerm.mk_int(str(i)))
        print(rval)
        return rval

    def comparison_forall_ints(self, tree):
        [var, expr] = tree
        return ddsolve.SmtComparison.mk_forall_ints(var, expr)

    def comparison_forall_reals(self, tree):
        [var, expr] = tree
        return ddsolve.SmtComparison.mk_forall_reals(var, expr)

    def expectation_ite(self, tree):
        a = tree[0]
        b = tree[1]
        c = tree[2]
        return a.ite(b, c)


def parse(prog, order = ddsolve.SmtVariableOrder()):
    parsed = smt_pgcl_parser.parse(prog)
    #print(parsed.pretty())
    #print("---")
    ddsolve_typer = ToDDSolveTypes(order, prog)
    parsed = ddsolve_typer.transform(parsed)
    #print(parsed.pretty())
    #print("---")
    return parsed, ddsolve_typer


class Verifier(Interpreter):
    def __init__(self, order):
        self.xadd = ddsolve.SmtXADD(order)
        self.one = self.xadd.get_term(ddsolve.SmtTerm.mk_real("1"))
        self.zero = self.xadd.get_term(ddsolve.SmtTerm.mk_real("0"))
        self.verified = "Unknown"
        self.print_nr = 0

    def verify(self, parsed):
        self.asserts = [(True, i.children[0]) for i in parsed.find_data("assert")]

        for i, a in self.asserts:
            ddsolve.SmtComparison.push_assumption(
                ddsolve.SmtComparison.WithParity(i, a)
            )

        self.visit(parsed)

        for a in self.asserts:
            ddsolve.SmtComparison.pop_assumption()
        return self.verified

    def _set_calculus(self, calculus):
        self.calculus = calculus
        self.liberal = calculus.endswith("lp")
        self.angelic = calculus.startswith("a")
        self.conditional = calculus.startswith("c")
        self.runtime = calculus.endswith("ert")
        self.ticktime = calculus.endswith("ett")
        if self.runtime or self.ticktime:
            self.angelic = not self.angelic

    def start(self, tree):
        self._set_calculus(tree.children[0].children[0])

        if self.calculus not in ["wp", "wlp", "ett"]:
            raise NotImplementedError("Only wp, wlp, ett")

        self.post_expectation = tree.children[-1]

        comperator = tree.children[0].children[1]
        pre_expectation_bound = tree.children[0].children[2]
        #fpprint("ofin.png", pre_expectation_bound, dict())
        #fpprint("pfin.png", self.post_expectation, dict())
        with ddsolve.LaceManager() as m:
            self.visit(tree.children[1])
            self.post_expectation = self.post_expectation.prune_full()
            with ddsolve.PrintModelsManager() as m:
                match comperator:
                    case "<":
                        self.verified = self.post_expectation < pre_expectation_bound
                    case "<=":
                        self.verified = self.post_expectation <= pre_expectation_bound
                    case ">":
                        self.verified = self.post_expectation > pre_expectation_bound
                    case ">=":
                        self.verified = self.post_expectation >= pre_expectation_bound
                    case "==":
                        self.verified = (
                            self.post_expectation <= pre_expectation_bound
                        ) and (self.post_expectation >= pre_expectation_bound)
                    case "!=":
                        self.verified = (
                            not self.post_expectation <= pre_expectation_bound
                        ) or (not self.post_expectation >= pre_expectation_bound)

    def statements(self, tree):
        for s in reversed(tree.children):
            self.visit(s)

    def while_kinduction(self, tree):
        take_extremum = True

        [invariant, iter_bound, unroll, loop_guard, loop_body] = tree.children

        generator = count() if iter_bound is None else range(int(iter_bound))
        generator = tqdm(generator)

        initial_expectation = self.post_expectation

        self.post_expectation = invariant
        for i in generator:
            self.visit(loop_body)
            self.post_expectation = loop_guard.ite(
                self.post_expectation, initial_expectation
            )
            self.post_expectation = self.post_expectation.prune_full()
            if self.liberal:
                if invariant <= self.post_expectation:
                    return
                if take_extremum:
                    self.post_expectation = self.post_expectation.max(invariant)
            else:
                if self.post_expectation <= invariant:
                    return
                if take_extremum:
                    self.post_expectation = self.post_expectation.min(invariant)
            self.post_expectation = self.post_expectation.prune_full()
        print("Warning: Could not prove the given invariant with kinduction!")
        self.visit(loop_body)
        self.post_expectation = loop_guard.ite(
            self.post_expectation, initial_expectation
        )
        self.post_expectation = self.post_expectation.prune_full()

    def while_unroll(self, tree):
        [iter_bound, loop_guard, loop_body] = tree.children
        generator = count() if iter_bound is None else range(int(iter_bound))
        generator = tqdm(generator)

        initial_expectation = self.post_expectation

        last_expectation = None
        self.post_expectation = self.one if self.liberal else self.zero
        for i in generator:
            last_expectation = self.post_expectation
            self.visit(loop_body)
            self.post_expectation = loop_guard.ite(
                self.post_expectation, initial_expectation
            )
            # fpprint(f"a{i}.png", self.post_expectation, dict())
            self.post_expectation = self.post_expectation.prune_full()
            # fpprint(f"o{i}.png", self.post_expectation, dict())
            if self.runtime:
                self.post_expectation = self.post_expectation + self.one
            if self.post_expectation == last_expectation:
                break

    def assign(self, tree):
        [a, b] = tree.children
        self.post_expectation = self.post_expectation.substitute(a, b)

    def full_array_assign(self, tree):
        [a, b] = tree.children
        self.post_expectation = self.post_expectation.substitute(a, b)

    def array_assign(self, tree):
        [a, idx, b] = tree.children
        self.post_expectation = self.post_expectation.substitute(
            a, a.array_store(idx, b)
        )

    def uniform_assign(self, tree):
        [var, low, high] = tree.children
        low, high = int(low), int(high)

        s = high - low
        if s <= 0:
            raise ValueError("A uniform distribution is empty.")

        tmp = self.post_expectation.scale(f"1/{s}")
        self.post_expectation = self.zero
        for i in range(low, high):
            self.post_expectation = self.post_expectation + tmp.substitute(
                var, ddsolve.SmtTerm.mk_int(str(i))
            )
            # self.post_expectation = self.post_expectation.prune_full()

    def prob_choice(self, tree):
        [if_body, p, else_body] = tree.children
        tmp = self.post_expectation
        self.visit(if_body)
        tmp, self.post_expectation = self.post_expectation, tmp
        self.visit(else_body)

        prob = self.xadd.get_term(p)
        cprob = self.one - prob

        self.post_expectation = tmp * prob + self.post_expectation * cprob

    def nd_choice(self, tree):
        [if_body, else_body] = tree.children
        tmp = self.post_expectation
        self.visit(if_body)
        tmp, self.post_expectation = self.post_expectation, tmp
        self.visit(else_body)

        if self.angelic:
            self.post_expectation = tmp.max(self.post_expectation)
        else:
            self.post_expectation = tmp.min(self.post_expectation)

    def if_(self, tree):
        has_else = len(tree.children) == 3
        guard = tree.children[0]
        if_body = tree.children[1]

        tmp = self.post_expectation
        self.visit(if_body)

        if_body = self.post_expectation

        if has_else:
            else_body = tree.children[2]
            self.post_expectation = tmp
            self.visit(else_body)
            else_body = self.post_expectation
        else:
            else_body = tmp
        self.post_expectation = guard.ite(if_body, else_body)

    def print(self, tree):
        fpprint(f"print_{self.print_nr}.png", self.post_expectation, dict())
        self.print_nr += 1

    def tick(self, tree):
        n = tree.children[0]
        self.post_expectation += n
