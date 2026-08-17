"""SMT-based PGCL parser and verifier."""

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
    """Transform SMT PGCL parse tree nodes into DDSolve decision diagram types.

    Handles variable, function, and array declarations, then converts
    expressions into SmtTerm, SmtComparison, SmtXBDD, and SmtXADD objects.

    Attributes:
        variables: Mapping of variable names to SmtTerm objects.
        funcs: Mapping of function names to SmtTerm objects.
        arrays: Mapping of array names to SmtTerm objects.
        xbdd: SmtXBDD instance for boolean decision diagrams.
        xadd: SmtXADD instance for arithmetic decision diagrams.
        to_comparison_type: Mapping of operator strings to ComparisonType enums.
    """

    def __init__(self, order, prog):
        """Initialize the SMT type converter.

        Args:
            order: The variable order for decision diagram construction.
            prog: The raw program string for error reporting.
        """
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
        """Collect variable declarations and register them in self.variables.

        Args:
            tree: The Lark parse tree containing variable_decl nodes.

        Raises:
            RuntimeError: If a variable is declared multiple times.
        """
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
        """Collect function declarations and register them in self.funcs.

        Args:
            tree: The Lark parse tree containing function_decl nodes.

        Raises:
            RuntimeError: If a function is declared multiple times.
        """
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
        """Collect array declarations and register them in self.arrays.

        Args:
            tree: The Lark parse tree containing array_decl nodes.

        Raises:
            RuntimeError: If an array is declared multiple times.
        """
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
        """Transform the parse tree after gathering declarations.

        Args:
            tree: The Lark parse tree to transform.

        Returns:
            The transformed tree.
        """
        self.gatherVars(tree)
        self.gatherFuncs(tree)
        self.gatherArrays(tree)
        return super().transform(tree)

    def variable_decl(self, tree):
        """Discard variable declaration nodes from the transformed tree.

        Args:
            tree: The Lark tree node for a variable declaration.

        Returns:
            Discard to remove this node from the tree.
        """
        return Discard

    def function_decl(self, tree):
        """Discard function declaration nodes from the transformed tree.

        Args:
            tree: The Lark tree node for a function declaration.

        Returns:
            Discard to remove this node from the tree.
        """
        return Discard

    def array_decl(self, tree):
        """Discard array declaration nodes from the transformed tree.

        Args:
            tree: The Lark tree node for an array declaration.

        Returns:
            Discard to remove this node from the tree.
        """
        return Discard

    def term_array(self, tree):
        """Convert an array reference to its SmtTerm.

        Args:
            tree: The Lark tree node containing the array name.

        Returns:
            The SmtTerm for the array.
        """
        id = tree[0]
        return self.arrays[id]

    def term_array_select(self, tree):
        """Convert an array select expression.

        Args:
            tree: The Lark tree node ([array, index]).

        Returns:
            The array_select result as an SmtTerm.
        """
        [a, idx] = tree
        return a.array_select(idx)

    def term_variable(self, tree):
        """Convert a variable reference to its SmtTerm.

        Args:
            tree: The Lark tree node containing the variable name.

        Returns:
            The SmtTerm for the variable.
        """
        id = tree[0]
        return self.variables[id]

    def term_function(self, tree):
        """Convert a function reference to its SmtTerm.

        Args:
            tree: The Lark tree node containing the function name.

        Returns:
            The SmtTerm for the function.
        """
        id = tree[0]
        return self.funcs[id]

    def term_function_call(self, tree):
        """Convert a function call to an SmtTerm via apply_func.

        Args:
            tree: The Lark tree node ([function, arg1, arg2, ...]).

        Returns:
            The result of applying the function to its arguments.
        """
        return tree[0].apply_func(tree[1:])

    def term_int(self, tree):
        """Convert an integer literal to an SmtTerm.

        Args:
            tree: The Lark tree node containing the integer value.

        Returns:
            The SmtTerm representing the integer.
        """
        val = tree[0]
        return ddsolve.SmtTerm.mk_int(val)

    def term_real(self, tree):
        """Convert a rational literal to an SmtTerm.

        Args:
            tree: The Lark tree node containing numerator and denominator.

        Returns:
            The SmtTerm representing the rational number.
        """
        val = tree[0] + "/" + tree[1]
        return ddsolve.SmtTerm.mk_real(val)

    def term_add(self, tree):
        """Add two terms.

        Args:
            tree: The Lark tree node ([left, right]).

        Returns:
            The sum of the two terms.
        """
        a = tree[0]
        b = tree[1]
        return a + b

    def term_mul(self, tree):
        """Multiply two terms.

        Args:
            tree: The Lark tree node ([left, right]).

        Returns:
            The product of the two terms.
        """
        a = tree[0]
        b = tree[1]
        return a * b

    def term_div(self, tree):
        """Divide two terms.

        Args:
            tree: The Lark tree node ([numerator, denominator]).

        Returns:
            The quotient of the two terms.
        """
        a = tree[0]
        b = tree[1]
        return a / b

    def term_sub(self, tree):
        """Subtract two terms.

        Args:
            tree: The Lark tree node ([left, right]).

        Returns:
            The difference of the two terms.
        """
        a = tree[0]
        b = tree[1]
        return a - b

    def term_to_real(self, tree):
        """Convert a term to a real-valued SmtTerm.

        Args:
            tree: The Lark tree node containing the term.

        Returns:
            The real-valued SmtTerm.
        """
        a = tree[0]
        return a.to_real()

    def term_to_int(self, tree):
        """Convert a term to an integer-valued SmtTerm.

        Args:
            tree: The Lark tree node containing the term.

        Returns:
            The integer-valued SmtTerm.
        """
        a = tree[0]
        return a.to_int()

    def expectation_term(self, tree):
        """Convert a term to an expectation via XADD.

        Args:
            tree: The Lark tree node containing the term.

        Returns:
            An XADD wrapping the term.
        """
        a = tree[0]
        return self.xadd.get_term(a)

    def comparison_base(self, tree):
        """Create a base SmtComparison from two terms and an operator.

        Args:
            tree: The Lark tree node ([left, operator, right]).

        Returns:
            An SmtComparison representing the comparison.
        """
        a = tree[0]
        op = tree[1]
        b = tree[2]
        return ddsolve.SmtComparison(a, b, self.to_comparison_type[op])

    def bool_comparison(self, tree):
        """Convert a comparison to an XBDD representation.

        Args:
            tree: The Lark tree node containing the comparison.

        Returns:
            An XBDD representing the boolean comparison.
        """
        a = tree[0]
        return self.xbdd.obtain_repr(a)

    def bool_and(self, tree):
        """Compute the conjunction of two boolean expressions.

        Args:
            tree: The Lark tree node ([left, right]).

        Returns:
            The XBDD representing the conjunction.
        """
        a = tree[0]
        b = tree[1]
        return a & b

    def bool_or(self, tree):
        """Compute the disjunction of two boolean expressions.

        Args:
            tree: The Lark tree node ([left, right]).

        Returns:
            The XBDD representing the disjunction.
        """
        a = tree[0]
        b = tree[1]
        return a | b

    def bool_negation(self, tree):
        """Negate a boolean expression.

        Args:
            tree: The Lark tree node containing the boolean.

        Returns:
            The negated XBDD.
        """
        a = tree[0]
        return ~a

    def bool_ite(self, tree):
        """Compute an if-then-else on boolean expressions.

        Args:
            tree: The Lark tree node ([guard, then_branch, else_branch]).

        Returns:
            The resulting XBDD.
        """
        a = tree[0]
        b = tree[1]
        c = tree[2]
        return a.ite(b, c)

    def bool_finite_forall(self, tree):
        """Evaluate finite forall over an integer range.

        Args:
            tree: The Lark tree node ([var, low, high, expr]).

        Returns:
            The XBDD representing the universally quantified expression.
        """
        [var, low, high, expr] = tree
        rval = self.xbdd.get_true()
        for i in range(int(low), int(high) + 1):
            rval = rval & expr.substitute(var, ddsolve.SmtTerm.mk_int(str(i)))
        print(rval)
        return rval

    def comparison_and(self, tree):
        """Compute the conjunction of two comparisons.

        Args:
            tree: The Lark tree node ([lhs, rhs]).

        Returns:
            The combined comparison.
        """
        [lhs, rhs] = tree
        return lhs & rhs

    def comparison_or(self, tree):
        """Compute the disjunction of two comparisons.

        Args:
            tree: The Lark tree node ([lhs, rhs]).

        Returns:
            The combined comparison.
        """
        [lhs, rhs] = tree
        return lhs & rhs

    def comparison_negation(self, tree):
        """Negate a comparison.

        Args:
            tree: The Lark tree node containing the comparison.

        Returns:
            The negated comparison.
        """
        a = tree[0]
        return ~a

    def comparison_finite_forall(self, tree):
        """Evaluate finite forall over an integer range for comparisons.

        Args:
            tree: The Lark tree node ([var, low, high, expr]).

        Returns:
            The SmtComparison representing the universally quantified expression.
        """
        [var, low, high, expr] = tree

        rval = expr.substitute(var, ddsolve.SmtTerm.mk_int(str(low)))
        for i in range(int(low) + 1, int(high) + 1):
            rval = rval & expr.substitute(var, ddsolve.SmtTerm.mk_int(str(i)))
        print(rval)
        return rval

    def comparison_forall_ints(self, tree):
        """Create a forall quantification over integers.

        Args:
            tree: The Lark tree node ([var, expr]).

        Returns:
            An SmtComparison with integer forall quantification.
        """
        [var, expr] = tree
        return ddsolve.SmtComparison.mk_forall_ints(var, expr)

    def comparison_forall_reals(self, tree):
        """Create a forall quantification over reals.

        Args:
            tree: The Lark tree node ([var, expr]).

        Returns:
            An SmtComparison with real forall quantification.
        """
        [var, expr] = tree
        return ddsolve.SmtComparison.mk_forall_reals(var, expr)

    def expectation_ite(self, tree):
        """Compute an if-then-else on expectations.

        Args:
            tree: The Lark tree node ([guard, then_branch, else_branch]).

        Returns:
            The resulting XADD.
        """
        a = tree[0]
        b = tree[1]
        c = tree[2]
        return a.ite(b, c)


def parse(prog, order = ddsolve.SmtVariableOrder()):
    """Parse an SMT PGCL program and convert it to DDSolve types.

    Args:
        prog: The raw SMT PGCL program string to parse.
        order: The variable order for decision diagram construction.

    Returns:
        tuple: (transformed_tree, ddsolve_typer) where ddsolve_typer
            contains the gathered variable, function, and array declarations.
    """
    parsed = smt_pgcl_parser.parse(prog)
    #print(parsed.pretty())
    #print("---")
    ddsolve_typer = ToDDSolveTypes(order, prog)
    parsed = ddsolve_typer.transform(parsed)
    #print(parsed.pretty())
    #print("---")
    return parsed, ddsolve_typer


class Verifier(Interpreter):
    """SMT-based probabilistic program verifier.

    Uses weakest precondition (wp), weakest liberal precondition (wlp),
    or expected time (ett) calculus to verify probabilistic programs
    with SMT terms, arrays, and functions.

    Attributes:
        xadd: SmtXADD instance for arithmetic decision diagrams.
        one: The one SmtTerm as an XADD.
        zero: The zero SmtTerm as an XADD.
        verified: The verification result ("True", "False", or "Unknown").
        print_nr: Counter for print statement output files.
    """

    def __init__(self, order):
        """Initialize the SMT verifier.

        Args:
            order: The variable order for decision diagram construction.
        """
        self.xadd = ddsolve.SmtXADD(order)
        self.one = self.xadd.get_term(ddsolve.SmtTerm.mk_real("1"))
        self.zero = self.xadd.get_term(ddsolve.SmtTerm.mk_real("0"))
        self.verified = "Unknown"
        self.print_nr = 0

    def verify(self, parsed):
        """Verify the parsed program against its assertions.

        Pushes all assertions as assumptions, visits the program tree
        to compute the post-expectation, then pops the assumptions
        and returns the verification result.

        Args:
            parsed: The transformed parse tree of the program.

        Returns:
            The verification result string ("True", "False", or "Unknown").
        """
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
        """Configure the verification calculus mode.

        Sets flags based on the calculus name:
            - liberal: ends with 'lp'
            - angelic: starts with 'a' (unless runtime/ticktime)
            - conditional: starts with 'c'
            - runtime: ends with 'ert'
            - ticktime: ends with 'ett'

        Args:
            calculus: The calculus annotation string (e.g., 'wp', 'wlp', 'ett').
        """
        self.calculus = calculus
        self.liberal = calculus.endswith("lp")
        self.angelic = calculus.startswith("a")
        self.conditional = calculus.startswith("c")
        self.runtime = calculus.endswith("ert")
        self.ticktime = calculus.endswith("ett")
        if self.runtime or self.ticktime:
            self.angelic = not self.angelic

    def start(self, tree):
        """Execute the main verification logic.

        Sets up the calculus, visits the program statements, and
        compares the computed post-expectation against the pre-bound.

        Args:
            tree: The Lark parse tree for the program.

        Raises:
            NotImplementedError: If the calculus is not wp, wlp, or ett.
        """
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
        """Visit all statements in reverse order (backward reasoning).

        Args:
            tree: The Lark tree node containing statement children.
        """
        for s in reversed(tree.children):
            self.visit(s)

    def while_kinduction(self, tree):
        """Verify a while loop using k-induction.

        Iteratively unrolls the loop body, checking if the invariant
        is preserved. Falls back to a single unroll if the bound is exceeded.

        Args:
            tree: The Lark tree node
                ([invariant, iter_bound, unroll, loop_guard, loop_body]).
        """
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
        """Verify a while loop by unrolling.

        Iteratively unrolls the loop body until the expectation stabilizes
        or the iteration bound is reached.

        Args:
            tree: The Lark tree node ([iter_bound, loop_guard, loop_body]).
        """
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
        """Handle variable assignment by substituting into post-expectation.

        Args:
            tree: The Lark tree node ([variable, expression]).
        """
        [a, b] = tree.children
        self.post_expectation = self.post_expectation.substitute(a, b)

    def full_array_assign(self, tree):
        """Handle full array assignment by substituting into post-expectation.

        Args:
            tree: The Lark tree node ([array, value]).
        """
        [a, b] = tree.children
        self.post_expectation = self.post_expectation.substitute(a, b)

    def array_assign(self, tree):
        """Handle array store assignment by substituting into post-expectation.

        Args:
            tree: The Lark tree node ([array, index, value]).
        """
        [a, idx, b] = tree.children
        self.post_expectation = self.post_expectation.substitute(
            a, a.array_store(idx, b)
        )

    def uniform_assign(self, tree):
        """Handle uniform random assignment over an integer range.

        Computes the expectation as the average over all values in [low, high).

        Args:
            tree: The Lark tree node ([variable, low, high]).

        Raises:
            ValueError: If the range is empty (low >= high).
        """
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
        """Handle probabilistic choice (bernoulli distribution).

        Computes the weighted expectation of the two branches.

        Args:
            tree: The Lark tree node ([if_body, probability, else_body]).
        """
        [if_body, p, else_body] = tree.children
        tmp = self.post_expectation
        self.visit(if_body)
        tmp, self.post_expectation = self.post_expectation, tmp
        self.visit(else_body)

        prob = self.xadd.get_term(p)
        cprob = self.one - prob

        self.post_expectation = tmp * prob + self.post_expectation * cprob

    def nd_choice(self, tree):
        """Handle nondeterministic choice.

        Takes the maximum (angelic) or minimum (demonic) of the two branches.

        Args:
            tree: The Lark tree node ([if_body, else_body]).
        """
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
        """Handle if-then-else constructs.

        Computes the expectation as the guarded if-then-else of the
        two branches.

        Args:
            tree: The Lark tree node ([guard, if_body]) or
                ([guard, if_body, else_body]).
        """
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
        """Handle print statements by exporting the current post-expectation.

        Args:
            tree: The Lark tree node for the print statement.
        """
        fpprint(f"print_{self.print_nr}.png", self.post_expectation, dict())
        self.print_nr += 1

    def tick(self, tree):
        """Handle tick (time cost) by incrementing the post-expectation.

        Args:
            tree: The Lark tree node containing the tick value.
        """
        n = tree.children[0]
        self.post_expectation += n
