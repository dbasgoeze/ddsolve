"""Dice language parser and evaluator for probabilistic programs."""

from lark import v_args, Lark
from lark.visitors import Interpreter

import ddsolve._ddsolve as ddsolve


dice_parser = Lark.open(
    "dice.lark",
    rel_to=__file__,
    propagate_positions=True,
    maybe_placeholders=True,
    parser="lalr",
)


class DiceEval(Interpreter):
    """Interpreter for Dice probabilistic programs.

    Evaluates Dice programs using backward reasoning over expectation
    and post-condition decision diagrams (XBDD/XADD).

    Attributes:
        v: Mapping of variable names to their assigned integer IDs.
        va: Next available variable ID.
        cs: Current substitution variable.
        tuple_choice: List of choice variable IDs for tuple constructs.
        to_comparison_type: Mapping of operator strings to ComparisonType enums.
        post_expect: Current post-expectation decision diagram.
        post_condition: Current post-condition decision diagram.
        xbdd: XBDD instance for boolean decision diagrams.
        xadd: XADD instance for arithmetic decision diagrams.
    """

    def __init__(self, order):
        """Initialize the Dice evaluator.

        Sets up initial post-expectation and post-condition decision diagrams
        representing the identity expectation (cs == 1) and true condition.

        Args:
            order: The variable order for decision diagram construction.
        """
        self.v = dict()
        self.va = 2
        self.cs = 1
        self.tuple_choice = [2]
        self.to_comparison_type = {
            "==": ddsolve.ComparisonType.kEquality,
            ">": ddsolve.ComparisonType.kGreaterThan,
            ">=": ddsolve.ComparisonType.kGreaterThanOrEqual,
            "!=": ddsolve.ComparisonType.kInequality,
            "<": ddsolve.ComparisonType.kLessThan,
            "<=": ddsolve.ComparisonType.kLessThanOrEqual,
        }

        with ddsolve.LaceManager() as _:
            self.xbdd = ddsolve.XBDD(order)
            self.xadd = ddsolve.XADD(order)
            self.post_expect = ddsolve.LinearTerm(
                [ddsolve.Product(self.cs, 1), ddsolve.Product(self.tuple_choice[0], -1)]
            )
            self.post_expect = ddsolve.LinearComparison(
                self.post_expect, ddsolve.ComparisonType.kEquality
            )
            self.post_expect = self.xbdd.obtain_repr(self.post_expect).ite(
                self.xadd.get_one(), self.xadd.get_zero()
            )
            self.post_condition = self.xadd.get_one()

    def eval(self, t):
        """Evaluate a parsed Dice program tree.

        Visits the tree to compute post-expectation and post-condition,
        then prunes both to their full form.

        Args:
            t: The Lark parse tree representing the program.
        """
        with ddsolve.LaceManager() as _:
            self.visit(t)
            self.post_expect = self.post_expect.prune_full()
            self.post_condition = self.post_condition.prune_full()

    def __default__(self, t):
        """Handle unexpected tree nodes.

        Args:
            t: The unexpected Lark tree node.
        """
        print("ALERT: " + str(t))

    def _get_free_var(self):
        """Allocate and return a new free variable ID.

        Returns:
            int: The next available variable ID.
        """
        self.va += 1
        return self.va

    def _free(self, v):
        """Mark a variable as free (no-op in current implementation).

        Args:
            v: The variable ID to free.
        """
        pass

    def start(self, t):
        """Entry point for visiting the program tree.

        Visits the first child of the start node and returns the
        resulting post-expectation.

        Args:
            t: The Lark tree node for the start of the program.

        Returns:
            The final post-expectation decision diagram.
        """
        self.visit(t.children[0])
        return self.post_expect

    def _register_ident(self, name):
        """Register an identifier with a new variable ID if not already registered.

        Args:
            name: The identifier string to register.
        """
        if name not in self.v.keys():
            self.v[name] = self._get_free_var()

    def ident(self, t):
        """Handle identifier nodes by registering the variable and substituting.

        Args:
            t: The Lark tree node containing the identifier.
        """
        (name,) = t.children
        self._register_ident(name)
        lt = ddsolve.LinearTerm([ddsolve.Product(self.v[name], 1)])
        self.post_expect = self.post_expect.substitute(self.cs, lt)
        self.post_condition = self.post_condition.substitute(self.cs, lt)

    def prob(self, t):
        """Parse a probability value and return it as a (numerator, denominator) tuple.

        Args:
            t: The Lark tree node containing the probability string.

        Returns:
            tuple[int, int]: The probability as (numerator, denominator).
        """
        c = t.children[0]
        if c[0] == "1":
            return 1, 1
        elif c[0] == ".":
            c = c[1:]
        else:
            c = c[2:]
        l = len(c)
        if l == 0:
            return 0, 1
        denum = "1" + (l * "0")
        return int(c), int(denum)

    def flip(self, t):
        """Handle probabilistic flip (bernoulli distribution) constructs.

        Computes the expectation as a weighted sum of the true and false branches,
        and similarly for the post-condition.

        Args:
            t: The Lark tree node containing the flip expression.
        """
        p = t.children[0]
        p = self.visit(p)
        ps = f"{p[0]}/{p[1]}"
        qs = f"{p[1] - p[0]}/{p[1]}"
        a = self.post_expect.substitute(
            self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 1)])
        ).scale(ps)
        b = self.post_expect.substitute(
            self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 0)])
        ).scale(qs)
        self.post_expect = (a + b).prune_full()

        a = self.post_condition.substitute(
            self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 1)])
        ).scale(ps)
        b = self.post_condition.substitute(
            self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 0)])
        ).scale(qs)
        self.post_condition = (a + b).prune_full()

    def let_(self, t):
        """Handle let-binding constructs by substituting and visiting bodies.

        Args:
            t: The Lark tree node containing the let expression (ident, e1, e2).
        """
        ident, e1, e2 = t.children
        (name,) = ident.children
        self._register_ident(name)
        self.visit(e2)
        lt = ddsolve.LinearTerm([ddsolve.Product(self.cs, 1)])
        self.post_expect = self.post_expect.substitute(self.v[name], lt)
        self.post_condition = self.post_condition.substitute(self.v[name], lt)
        self.visit(e1)

    def bin_op(self, t):
        """Handle boolean binary operators (&&, ||).

        Constructs a linear comparison for the boolean combination, uses it as
        an if-then-else guard over the post-expectation and post-condition,
        then recursively visits both operands.

        Args:
            t: The Lark tree node (a, operator, b).
        """
        a, o, b = t.children
        v2, v3 = self._get_free_var(), self._get_free_var()
        if o == "&&":
            lt = ddsolve.LinearTerm(
                [ddsolve.Product(v2, 1), ddsolve.Product(v3, 1), ddsolve.Product(0, -2)]
            )
        elif o == "||":
            lt = ddsolve.LinearTerm(
                [ddsolve.Product(v2, 1), ddsolve.Product(v3, 1), ddsolve.Product(0, -1)]
            )
        lc = ddsolve.LinearComparison(lt, ddsolve.ComparisonType.kGreaterThanOrEqual)
        tmp = self.xbdd.obtain_repr(lc)
        self.post_expect = tmp.ite(
            self.post_expect.substitute(
                self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 1)])
            ),
            self.post_expect.substitute(
                self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 0)])
            ),
        )
        self.post_condition = tmp.ite(
            self.post_condition.substitute(
                self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 1)])
            ),
            self.post_condition.substitute(
                self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 0)])
            ),
        )
        del tmp
        old_cs = self.cs
        self.cs = v2
        self.visit(a)
        self.cs = v3
        self.visit(b)
        self.cs = old_cs
        self._free(v2)
        self._free(v3)

    def arith_op(self, t):
        """Handle arithmetic binary operators (+, -).

        Substitutes the current variable with the arithmetic result, then
        recursively visits both operands.

        Args:
            t: The Lark tree node (a, operator, b).
        """
        a, o, b = t.children
        v2, v3 = self._get_free_var(), self._get_free_var()
        if o == "+":
            lt = ddsolve.LinearTerm([ddsolve.Product(v2, 1), ddsolve.Product(v3, 1)])
        elif o == "-":
            lt = ddsolve.LinearTerm([ddsolve.Product(v2, 1), ddsolve.Product(v3, -1)])
        self.post_expect = self.post_expect.substitute(self.cs, lt)
        self.post_condition = self.post_condition.substitute(self.cs, lt)
        old_cs = self.cs
        self.cs = v2
        self.visit(a)
        self.cs = v3
        self.visit(b)
        self.cs = old_cs
        self._free(v2)
        self._free(v3)

    def comp_op(self, t):
        """Handle comparison operators (==, >, >=, !=, <, <=).

        Constructs a linear comparison between two operands, uses it as
        an if-then-else guard over the post-expectation and post-condition,
        then recursively visits both operands.

        Args:
            t: The Lark tree node (a, operator, b).
        """
        a, o, b = t.children
        v2, v3 = self._get_free_var(), self._get_free_var()
        lat = ddsolve.LinearTerm([ddsolve.Product(v2, 1)])
        lbt = ddsolve.LinearTerm([ddsolve.Product(v3, 1)])
        lc = ddsolve.LinearComparison(lat, lbt, self.to_comparison_type[o])
        tmp = self.xbdd.obtain_repr(lc)
        self.post_expect = tmp.ite(
            self.post_expect.substitute(
                self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 1)])
            ),
            self.post_expect.substitute(
                self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 0)])
            ),
        )
        self.post_condition = tmp.ite(
            self.post_condition.substitute(
                self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 1)])
            ),
            self.post_condition.substitute(
                self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 0)])
            ),
        )
        del tmp
        old_cs = self.cs
        self.cs = v2
        self.visit(a)
        self.cs = v3
        self.visit(b)
        self.cs = old_cs
        self._free(v2)
        self._free(v3)

    def pair(self, t):
        """Handle tuple/pair constructs with inequality constraints.

        Creates a new choice variable and uses an inequality comparison as
        an if-then-else guard to substitute into the post-expectation and
        post-condition.

        Args:
            t: The Lark tree node (a, b) representing the pair.
        """
        a, b = t.children
        cs, ncs = self.cs, self._get_free_var()
        nc = self._get_free_var()
        self.tuple_choice.append(nc)
        lat = ddsolve.LinearTerm([ddsolve.Product(ncs, 1), ddsolve.Product(nc, -1)])
        lc = ddsolve.LinearComparison(lat, ddsolve.ComparisonType.kInequality)
        tmp = self.xbdd.obtain_repr(lc)
        self.post_expect = tmp.ite(
            self.post_expect.substitute(
                self.tuple_choice[0],
                ddsolve.LinearTerm([ddsolve.Product(1, 1), ddsolve.Product(0, 1)]),
            ),
            self.post_expect,
        )
        self.post_condition = tmp.ite(
            self.post_condition.substitute(
                self.tuple_choice[0],
                ddsolve.LinearTerm([ddsolve.Product(1, 1), ddsolve.Product(0, 1)]),
            ),
            self.post_condition,
        )

        del tmp
        self.cs = ncs
        self.visit(b)
        self.cs = cs
        self.visit(a)

    def if_(self, t):
        """Handle if-then-else constructs.

        Visits the then and else branches separately, then combines the
        results using the condition as an if-then-else guard.

        Args:
            t: The Lark tree node (condition, then_branch, else_branch).
        """
        a, b, c = t.children

        pec, pcc = self.post_expect, self.post_condition
        self.visit(b)
        peb, pcb = self.post_expect, self.post_condition
        self.post_expect, self.post_condition = pec, pcc
        self.visit(c)
        pec, pcc = self.post_expect, self.post_condition

        lt = ddsolve.LinearTerm([ddsolve.Product(self.cs, 1)])
        lc = ddsolve.LinearComparison(lt, ddsolve.ComparisonType.kGreaterThan)
        tmp = self.xbdd.obtain_repr(lc)

        self.post_expect = tmp.ite(peb, pec)
        self.post_condition = tmp.ite(pcb, pcc)
        del tmp
        self.visit(a)

    def observe(self, t):
        """Handle observe (sampling) constructs.

        Zeros out post-expectation and post-condition when the condition
        is false, then substitutes the observed value.

        Args:
            t: The Lark tree node containing the observed expression.
        """
        (a,) = t.children
        lat = ddsolve.LinearTerm([ddsolve.Product(self.cs, 1)])
        lc = ddsolve.LinearComparison(lat, ddsolve.ComparisonType.kGreaterThan)
        tmp = self.xbdd.obtain_repr(lc)
        self.post_expect = tmp.ite(self.post_expect, self.xadd.get_zero())
        self.post_condition = tmp.ite(self.post_condition, self.xadd.get_zero())
        del tmp
        self.visit(a)
        self.post_expect = self.post_expect.substitute(
            self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 1)])
        )
        self.post_condition = self.post_condition.substitute(
            self.cs, ddsolve.LinearTerm([ddsolve.Product(0, 1)])
        )

    def not_(self, t):
        """Handle boolean negation.

        Substitutes the current variable with its negation (cs -> -cs + 1)
        in both post-expectation and post-condition, then visits the operand.

        Args:
            t: The Lark tree node containing the negated expression.
        """
        (a,) = t.children
        self.post_expect = self.post_expect.substitute(
            self.cs,
            ddsolve.LinearTerm([ddsolve.Product(self.cs, "-1"), ddsolve.Product(0, 1)]),
        )
        self.post_condition = self.post_condition.substitute(
            self.cs,
            ddsolve.LinearTerm([ddsolve.Product(self.cs, "-1"), ddsolve.Product(0, 1)]),
        )
        self.visit(a)

    def discrete(self, t):
        """Handle discrete probability distributions (choice over multiple outcomes).

        Computes the weighted sum of post-expectation and post-condition for
        each outcome, scaled by its probability.

        Args:
            t: The Lark tree node containing the probability assignments.
        """
        probs = [self.visit(c) for c in t.children]
        #probs = [f"{a}/{b}" for a, b in probs]
        tmp = self.xadd.get_zero()
        for i, p in enumerate(probs):
            if p[0] != 0:
                p = f'{p[0]}/{p[1]}'
                tmp += self.post_expect.substitute(
                    self.cs, ddsolve.LinearTerm([ddsolve.Product(0, i)])
                ).prune_full().scale(p)
                tmp = tmp.prune_full()
        self.post_expect = tmp

        tmp = self.xadd.get_zero()
        for i, p in enumerate(probs):
            if p[0] != 0:
                p = f'{p[0]}/{p[1]}'
                tmp += self.post_condition.substitute(
                    self.cs, ddsolve.LinearTerm([ddsolve.Product(0, i)])
                ).prune_full().scale(p)
                tmp = tmp.prune_full()
        self.post_condition = tmp

    def int(self, t):
        """Handle integer literal values by substituting the current variable.

        Args:
            t: The Lark tree node containing the integer value.
        """
        (i,) = t.children
        self.post_expect = self.post_expect.substitute(
            self.cs, ddsolve.LinearTerm([ddsolve.Product(0, i)])
        )
        self.post_condition = self.post_condition.substitute(
            self.cs, ddsolve.LinearTerm([ddsolve.Product(0, i)])
        )
