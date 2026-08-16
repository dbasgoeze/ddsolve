from lark import Visitor, Transformer, Discard, v_args, Tree, Lark
from weakref import proxy

import ddsolve._ddsolve as ddsolve


class GeneralizeTerms(Visitor):
    def __default__(self, tree):
        tree.data = tree.data.replace("fterm", "term").replace("fbool", "bool")


class AllocateVariables(Transformer):
    def __init__(self, prog):
        self.prog = prog

    def transform(self, tree):
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
        if tree.children[0].value in self.to_var:
            return self.to_var[tree.children[0].value]
        return tree

    @v_args(tree=True)
    def assign(self, tree):
        if tree.children[0] == "const":
            return Discard
        return tree


class ToDDSolveTypes(Transformer):
    def __init__(self, order, prog):
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
        if len(c) == 2:
            return ddsolve.LinearTerm([ddsolve.Product(c[1], c[0] if c[0] else 1)])
        return ddsolve.LinearTerm([ddsolve.Product(0, c[0])])

    def term_mul(self, c):
        if c[0]:
            return c[1].scale(c[0])
        else:
            return c[1]

    def term_plus(self, c):
        assert len(c) == 2
        return c[0] + c[1]

    def term_minus(self, c):
        assert len(c) == 2
        return c[0] - c[1]

    def rational(self, c):
        return c[0] if len(c) == 1 else c[0] + "/" + c[1]

    @v_args(meta=True)
    def probability(self, meta, c):
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
        assert len(c) == 3
        return self.xbdd.obtain_repr(
            ddsolve.LinearComparison(c[0], c[2], self.to_comparison_type[c[1]])
        )

    def bool_expr_and(self, c):
        assert len(c) == 2
        return c[0] & c[1]

    def bool_expr_or(self, c):
        assert len(c) == 2
        return c[0] | c[1]

    def bool_expr_not(self, c):
        assert len(c) == 1
        return ~c[0]

    def bool_expr_ite(self, c):
        assert len(c) == 3
        return c[0].ite(c[1], c[2])


    def expectation_base(self, c):
        if isinstance(c[0], str):
            return self.xadd.get_term(ddsolve.LinearTerm([ddsolve.Product(0, c[0])]))
        return self.xadd.get_term(ddsolve.LinearTerm([ddsolve.Product(c[0], 1)]))

    def expectation_ite(self, c):
        assert len(c) == 3
        return c[0].ite(c[1], c[2])

    def expectation_infty(self, c):
        assert len(c) == 0
        return self.xadd.get_infty()

    def expectation_mul(self, c):
        a,b = c
        return b.scale(a)

    def expectation_add(self, c):
        a,b = c
        return a + b

    def expectation_sub(self, c):
        a,b = c
        return a - b



    def distribution_explicit(self, c):
        rval = [i.children for i in c]
        rval = [(i, ddsolve.LinearTerm([ddsolve.Product(0, j)])) for i, j in rval]
        k = self.zero
        for i in rval:
            k = k + self.one.scale(f"{i[0][0]}/{i[0][1]}")
        if k != self.one:
            raise ValueError("An explicit distribution does not sum to 1.")
        return rval

    def distribution_uniform(self, c):
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
    variable_allocator = AllocateVariables(pgcl_prog)
    type_converter = ToDDSolveTypes(order, pgcl_prog)
    rval = pgcl_parser.parse(pgcl_prog)
    GeneralizeTerms().visit(rval)
    rval = variable_allocator.transform(rval)
    rval = type_converter.transform(rval)
    return rval, {j:i for i,j in variable_allocator.to_var.items()}
