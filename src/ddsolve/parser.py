"""PGCL parser that transforms programs into DDSolve decision diagram types."""

from lark import Visitor, Transformer, Discard, v_args, Tree, Lark
from weakref import proxy

import ddsolve._ddsolve as ddsolve


class GeneralizeTerms(Visitor):
    """Generalize term and bool node types in the parse tree.

    Renames 'fterm' nodes to 'term' and 'fbool' nodes to 'bool'.
    """

    def __default__(self, tree):
        tree.data = tree.data.replace("fterm", "term").replace("fbool", "bool")


class AllocateVariables(Transformer):
    """Allocate variable IDs and substitute constants in the parse tree.

    Assigns sequential IDs to program variables (starting from 1) and
    handles constant expressions by substituting their definitions.

    Attributes:
        const_exprs: Mapping of constant names to their expression trees.
        const_ids: Set of already-declared constant names.
        to_var: Mapping of variable names to their allocated IDs.
    """

    def __init__(self, prog):
        """Initialize the variable allocator.

        Args:
            prog: The raw program string for error reporting.
        """
        self.prog = prog

    def transform(self, tree):
        """Transform the parse tree by allocating variables and substituting constants.

        Args:
            tree: The Lark parse tree to transform.

        Returns:
            The transformed tree with variables allocated and constants substituted.

        Raises:
            ValueError: If a constant is reassigned.
        """
        self.const_exprs = {}
        self.const_ids = set()
        ids = {i.children[0].value for i in tree.find_data("identifier")}
        for i in filter(lambda x: x.data == "assign", tree.iter_subtrees_topdown()):
            if i.children[0] == "const":
                id = i.children[1].children[0].value
                if id in self.const_ids:
                    raise ValueError(
                        f'Const reassignment in line {tree.meta.line} "{self.prog[tree.meta.start_pos:tree.meta.end_pos]}"'
                    )
                self.const_ids.add(id)
                self.const_exprs[id] = i.children[2]
                if id in ids:
                    ids.remove(id)
        self.to_var = {j: i + 1 for i, j in enumerate(sorted(ids))}
        print(f'to_var = { {j: "x_" + str(i + 1) for i, j in enumerate(sorted(ids))} }')
        tmp = {}
        for i, j in self.const_exprs.items():
            tmp[i] = Transformer.transform(self, j)
        self.const_exprs = tmp
        return Transformer.transform(self, tree)

    @v_args(tree=True)
    def term_base(self, tree):
        """Transform term_base nodes, substituting constant expressions.

        Args:
            tree: The Lark tree node for a term_base.

        Returns:
            The transformed tree with constant expressions substituted.
        """
        last = tree.children[-1]

        if (
            isinstance(last, Tree)
            and last.data == "identifier"
            and last.children[0] in self.const_exprs
        ):
            return Tree(
                "term_mul", [tree.children[0], self.const_exprs[last.children[0]]]
            )
        return tree

    @v_args(tree=True)
    def identifier(self, tree):
        """Transform identifier nodes by replacing names with allocated variable IDs.

        Args:
            tree: The Lark tree node for an identifier.

        Returns:
            The variable ID integer, or the original tree if not a program variable.
        """
        if tree.children[0].value in self.to_var:
            return self.to_var[tree.children[0].value]
        return tree

    @v_args(tree=True)
    def assign(self, tree):
        """Transform assign nodes, discarding constant assignments.

        Args:
            tree: The Lark tree node for an assignment.

        Returns:
            Discard if this is a constant assignment, otherwise the original tree.
        """
        if tree.children[0] == "const":
            return Discard
        return tree


class ToDDSolveTypes(Transformer):
    """Transform parse tree nodes into DDSolve decision diagram types.

    Converts parsed expression trees into LinearTerm, SmtComparison, XADD,
    and XBDD objects for use by the verifier.

    Attributes:
        xbdd: XBDD instance for boolean decision diagrams.
        xadd: XADD instance for arithmetic decision diagrams.
        zero: The zero XADD value.
        one: The one XADD value.
        to_comparison_type: Mapping of operator strings to ComparisonType enums.
    """

    def __init__(self, order, prog):
        """Initialize the type converter.

        Args:
            order: The variable order for decision diagram construction.
            prog: The raw program string for error reporting.
        """
        self.prog = prog
        self.xbdd = ddsolve.XBDD(order)
        self.xadd = ddsolve.XADD(order)
        self.zero = self.xadd.get_zero()
        self.one = self.xadd.get_one()
        self.to_comparison_type = {
            "==": ddsolve.ComparisonType.kEquality,
            ">": ddsolve.ComparisonType.kGreaterThan,
            ">=": ddsolve.ComparisonType.kGreaterThanOrEqual,
            "!=": ddsolve.ComparisonType.kInequality,
            "<": ddsolve.ComparisonType.kLessThan,
            "<=": ddsolve.ComparisonType.kLessThanOrEqual,
        }

    def term_base(self, c):
        """Convert a term_base node to a LinearTerm.

        Args:
            c: The children of the term_base node.

        Returns:
            A LinearTerm representing the base term.
        """
        if len(c) == 2:
            return ddsolve.LinearTerm([ddsolve.Product(c[1], c[0] if c[0] else 1)])
        return ddsolve.LinearTerm([ddsolve.Product(0, c[0])])

    def term_mul(self, c):
        """Scale a term by a coefficient.

        Args:
            c: A tuple of (coefficient, term).

        Returns:
            The scaled LinearTerm.
        """
        if c[0]:
            return c[1].scale(c[0])
        else:
            return c[1]

    def term_plus(self, c):
        """Add two terms together.

        Args:
            c: A tuple of (left_term, right_term).

        Returns:
            The sum of the two terms.
        """
        assert len(c) == 2
        return c[0] + c[1]

    def term_minus(self, c):
        """Subtract two terms.

        Args:
            c: A tuple of (left_term, right_term).

        Returns:
            The difference of the two terms.
        """
        assert len(c) == 2
        return c[0] - c[1]

    def rational(self, c):
        """Format a rational number as a string.

        Args:
            c: The children of the rational node.

        Returns:
            The rational number as a fraction string.
        """
        return c[0] if len(c) == 1 else c[0] + "/" + c[1]

    @v_args(meta=True)
    def probability(self, meta, c):
        """Parse and validate a probability value.

        Args:
            meta: The Lark metadata for error reporting.
            c: The children of the probability node.

        Returns:
            A tuple of (numerator, denominator).

        Raises:
            ValueError: If the probability exceeds 1.
        """
        c = c[0].split("/")
        assert len(c) == 2
        a = int(c[0])
        b = int(c[1])
        if a > b:
            raise ValueError(
                f"Probability ({self.prog[meta.start_pos:meta.end_pos]}) > 1 at line {meta.line}"
            )
        return (a, b)

    def bool_expr_base(self, c):
        """Convert a boolean expression base to an XBDD representation.

        Args:
            c: A tuple of (left_term, operator, right_term).

        Returns:
            An XBDD representing the comparison.
        """
        assert len(c) == 3
        return self.xbdd.obtain_repr(
            ddsolve.LinearComparison(c[0], c[2], self.to_comparison_type[c[1]])
        )

    def bool_expr_and(self, c):
        """Compute the conjunction of two boolean expressions.

        Args:
            c: A tuple of (left_bool, right_bool).

        Returns:
            The XBDD representing the conjunction.
        """
        assert len(c) == 2
        return c[0] & c[1]

    def bool_expr_or(self, c):
        """Compute the disjunction of two boolean expressions.

        Args:
            c: A tuple of (left_bool, right_bool).

        Returns:
            The XBDD representing the disjunction.
        """
        assert len(c) == 2
        return c[0] | c[1]

    def bool_expr_not(self, c):
        """Negate a boolean expression.

        Args:
            c: A tuple containing the boolean to negate.

        Returns:
            The negated XBDD.
        """
        assert len(c) == 1
        return ~c[0]

    def bool_expr_ite(self, c):
        """Compute an if-then-else on boolean expressions.

        Args:
            c: A tuple of (guard, then_branch, else_branch).

        Returns:
            The resulting XBDD.
        """
        assert len(c) == 3
        return c[0].ite(c[1], c[2])


    def expectation_base(self, c):
        """Convert an expectation base to an XADD representation.

        Args:
            c: The children of the expectation base node.

        Returns:
            An XADD representing the expectation value.
        """
        if isinstance(c[0], str):
            return self.xadd.get_term(ddsolve.LinearTerm([ddsolve.Product(0, c[0])]))
        return self.xadd.get_term(ddsolve.LinearTerm([ddsolve.Product(c[0], 1)]))

    def expectation_ite(self, c):
        """Compute an if-then-else on expectations.

        Args:
            c: A tuple of (guard, then_branch, else_branch).

        Returns:
            The resulting XADD.
        """
        assert len(c) == 3
        return c[0].ite(c[1], c[2])

    def expectation_infty(self, c):
        """Return the infinity XADD value.

        Returns:
            An XADD representing infinity.
        """
        assert len(c) == 0
        return self.xadd.get_infty()

    def expectation_mul(self, c):
        """Scale an expectation by a probability.

        Args:
            c: A tuple of (probability, expectation).

        Returns:
            The scaled XADD.
        """
        a, b = c
        return b.scale(a)

    def expectation_add(self, c):
        """Add two expectations.

        Args:
            c: A tuple of (left_expectation, right_expectation).

        Returns:
            The sum of the two XADDs.
        """
        a, b = c
        return a + b

    def expectation_sub(self, c):
        """Subtract two expectations.

        Args:
            c: A tuple of (left_expectation, right_expectation).

        Returns:
            The difference of the two XADDs.
        """
        a, b = c
        return a - b

    def distribution_explicit(self, c):
        """Convert an explicit distribution to a list of (probability, term) pairs.

        Validates that the probabilities sum to 1.

        Args:
            c: The children of the distribution node.

        Returns:
            A list of (probability_tuple, LinearTerm) pairs.

        Raises:
            ValueError: If the distribution probabilities do not sum to 1.
        """
        rval = [i.children for i in c]
        rval = [(i, ddsolve.LinearTerm([ddsolve.Product(0, j)])) for i, j in rval]
        k = self.zero
        for i in rval:
            k = k + self.one.scale(f"{i[0][0]}/{i[0][1]}")
        if k != self.one:
            raise ValueError("An explicit distribution does not sum to 1.")
        return rval

    def distribution_uniform(self, c):
        """Convert a uniform distribution range to a list of (probability, term) pairs.

        Args:
            c: A tuple of (start, end) integers.

        Returns:
            A list of (probability_tuple, LinearTerm) pairs.

        Raises:
            ValueError: If the distribution range is empty.
        """
        start, end = [int(i) for i in c]
        size = end - start
        rval = [
            ((1, size), ddsolve.LinearTerm([ddsolve.Product(0, i + start)]))
            for i in range(size)
        ]
        if not rval:
            raise ValueError("A uniform distribution is empty.")
        return rval


pgcl_parser = Lark.open(
    "pgcl.lark",
    rel_to=__file__,
    propagate_positions=True,
    maybe_placeholders=True,
    parser="lalr",
)


def parse(pgcl_prog, order=ddsolve.VariableOrder()):
    """Parse a PGCL program and convert it to DDSolve types.

    The parsing pipeline:
        1. Parse the raw program string into a Lark tree.
        2. Generalize term/bool node types.
        3. Allocate variable IDs and substitute constants.
        4. Convert nodes to DDSolve decision diagram types.

    Args:
        pgcl_prog: The raw PGCL program string to parse.
        order: The variable order for decision diagram construction.

    Returns:
        tuple: (transformed_tree, to_varname_dict) where to_varname_dict
            maps variable IDs to their original names.

    Raises:
        ValueError: If a constant is reassigned or a probability exceeds 1.
    """
    variable_allocator = AllocateVariables(pgcl_prog)
    type_converter = ToDDSolveTypes(order, pgcl_prog)
    rval = pgcl_parser.parse(pgcl_prog)
    GeneralizeTerms().visit(rval)
    rval = variable_allocator.transform(rval)
    rval = type_converter.transform(rval)
    return rval, {j:i for i,j in variable_allocator.to_var.items()}
