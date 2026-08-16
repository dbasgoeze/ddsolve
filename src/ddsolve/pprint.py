try:
    import graphviz
except ImportError:
    print(
        "Missing Dependency to graphviz output! Probably the python graphviz pkg itself."
    )
import re
import tempfile

label_pattern = re.compile(r'(?m)(^\d+ \[label=")(\d+)("\])')
variable_pattern = re.compile(r"\bx(\d+)")

def fpprint(path, dd, to_varname):
    def get_comparisons(dd):
        variables = dd.get_order().get_variables()
        return [(v, dd.get_order().get_comparison(v)) for v in variables]

    comps = dict(get_comparisons(dd))

    def insert_comparisons(match):
        pre, num, pos = match.group(1, 2, 3)
        return pre + str(comps[int(num)]) + pos

    def insert_varnames(match):
        id = int(match.group(1))
        return to_varname[id]

    dot = label_pattern.sub(insert_comparisons, dd.to_dot())
    dot = variable_pattern.sub(insert_varnames, dot)

    try:
        src = graphviz.Source(dot)
        with tempfile.TemporaryDirectory() as tmpdir:
            src.render(filename=f"{tmpdir}/temp_source.gv", outfile=path)
    except NameError:
        print(path)
        with open(path, "w") as f:
            f.write(dot)
