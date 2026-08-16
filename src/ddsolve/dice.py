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
    def __init__(self, order):
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
        with ddsolve.LaceManager() as _:
            self.visit(t)
            self.post_expect = self.post_expect.prune_full()
            self.post_condition = self.post_condition.prune_full()

    def __default__(self, t):
        print("ALERT: " + str(t))

    def _get_free_var(self):
        self.va += 1
        return self.va

    def _free(self, v):
        pass

    def start(self, t):
        self.visit(t.children[0])
        return self.post_expect

    def _register_ident(self, name):
        if name not in self.v.keys():
            self.v[name] = self._get_free_var()

    def ident(self, t):
        (name,) = t.children
        self._register_ident(name)
        lt = ddsolve.LinearTerm([ddsolve.Product(self.v[name], 1)])
        self.post_expect = self.post_expect.substitute(self.cs, lt)
        self.post_condition = self.post_condition.substitute(self.cs, lt)

    def prob(self, t):
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
        ident, e1, e2 = t.children
        (name,) = ident.children
        self._register_ident(name)
        self.visit(e2)
        lt = ddsolve.LinearTerm([ddsolve.Product(self.cs, 1)])
        self.post_expect = self.post_expect.substitute(self.v[name], lt)
        self.post_condition = self.post_condition.substitute(self.v[name], lt)
        self.visit(e1)

    def bin_op(self, t):
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
        (i,) = t.children
        self.post_expect = self.post_expect.substitute(
            self.cs, ddsolve.LinearTerm([ddsolve.Product(0, i)])
        )
        self.post_condition = self.post_condition.substitute(
            self.cs, ddsolve.LinearTerm([ddsolve.Product(0, i)])
        )
