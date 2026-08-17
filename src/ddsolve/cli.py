"""Command-line interface for the ddsolve probabilistic program verifier."""

import argparse
from sys import exit, setrecursionlimit
from pathlib import Path

from ddsolve._ddsolve import VariableOrder, XADD, SmtXADD

setrecursionlimit(10000000)


def main():
    """Parse command-line arguments and execute the verifier in the selected mode.

    Supports three modes:
        - pgcl: Linear term PGCL mode (default)
        - smt: SMT term PGCL mode
        - dice: Dice mode

    Raises:
        SystemExit: Exits with code 0 if verification succeeds, 1 if it fails,
            or 2 if an unknown mode is specified.
    """

def main():
    arg_parser = argparse.ArgumentParser(
        prog="ddsolve", description="Expectation-Based Probabilistic Program Verifier"
    )
    arg_parser.add_argument(
        "program_file", help="The program under test.", type=argparse.FileType("r")
    )
    arg_parser.add_argument(
        "-o",
        "--output",
        help="Ouput the final post-expectation DD as a graph.",
        metavar="FILE",
        type=Path,
    )
    group = arg_parser.add_mutually_exclusive_group()
    group.add_argument(
        "-p",
        "--pgcl",
        dest="mode",
        action="store_const",
        const="pgcl",
        help="use linear term pgcl mode (default)",
    )
    group.add_argument(
        "-smt",
        "--smtpgcl",
        dest="mode",
        action="store_const",
        const="smt",
        help="use smt term pgcl mode",
    )
    group.add_argument(
        "-d",
        "--dice",
        dest="mode",
        action="store_const",
        const="dice",
        help="Use Dice mode",
    )
    arg_parser.set_defaults(mode="pgcl")
    args = arg_parser.parse_args()

    prog = args.program_file.read()

    if args.mode == "pgcl":
        from ddsolve.parser import parse
        from ddsolve.wp import Verifier
        order = VariableOrder()
        ast, to_varname = parse(prog, order)
        verifier = Verifier(order)
        verifier.verify(ast)

        if args.output:
            from ddsolve.pprint import fpprint
            output_file = args.output
            post_expectation = verifier.post_expectation
            if isinstance(post_expectation, XADD):
                fpprint(output_file, post_expectation, to_varname)
            else:
                fpprint(
                    output_file.with_name("wp_" + output_file.name),
                    post_expectation[0],
                    to_varname,
                )
                fpprint(
                    output_file.with_name("wlp_" + output_file.name),
                    post_expectation[1],
                    to_varname,
                )

        if verifier.verified:
            print("True")
            exit(0)
        else:
            print("False")
            exit(1)
    elif args.mode == "smt":
        from ddsolve.smt_parser import parse
        from ddsolve.smt_parser import Verifier
        from ddsolve import SmtVariableOrder
        order = SmtVariableOrder()
        ast, typer = parse(prog, order)
        verifier = Verifier(order)
        verifier.verify(ast)

        if args.output:
            from ddsolve.pprint import fpprint
            output_file = args.output
            post_expectation = verifier.post_expectation
            if isinstance(post_expectation, SmtXADD):
                fpprint(output_file, post_expectation, dict())
            else:
                fpprint(
                    output_file.with_name("wp_" + output_file.name),
                    post_expectation[0],
                    to_varname,
                )
                fpprint(
                    output_file.with_name("wlp_" + output_file.name),
                    post_expectation[1],
                    to_varname,
                )

        if verifier.verified:
            print("True")
            exit(0)
        else:
            print("False")
            exit(1)
    elif args.mode == "dice":
        from ddsolve.dice import dice_parser, DiceEval
        order = VariableOrder()
        ast = dice_parser.parse(prog)
        dice_eval = DiceEval(order)
        dice_eval.eval(ast)

        if args.output:
            from ddsolve.pprint import fpprint

            to_varname = { j : i for i,j in dice_eval.v.items() }
            for i,j in enumerate(dice_eval.tuple_choice):
                to_varname[j] = f'_result_{i}'

            output_file = args.output
            fpprint(
                output_file.with_name("wp_" + output_file.name),
                dice_eval.post_expect,
                to_varname,
            )
            fpprint(
                output_file.with_name("wlp_" + output_file.name),
                dice_eval.post_condition,
                to_varname,
            )
    else:
        print("This should not happen! Unknown mode set")
        exit(2)
