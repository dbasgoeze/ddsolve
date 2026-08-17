"""Pretty-print decision diagrams as graphviz visualizations."""

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
    """Write a decision diagram to a file as a labeled graph.

    Renders the decision diagram as a graphviz DOT file, replacing
    internal node labels with comparison descriptions and variable
    IDs with human-readable names. Falls back to writing raw DOT
    if graphviz rendering is unavailable.

    Args:
        path: The output file path for the rendered diagram.
        dd: The decision diagram (XADD or SmtXADD) to render.
        to_varname: A dict mapping variable IDs to their display names.
    """

    def get_comparisons(dd):
        """Extract variable-comparison pairs from the diagram's variable order.

        Args:
            dd: The decision diagram.

        Returns:
            A list of (variable_id, comparison_string) tuples.
        """
        variables = dd.get_order().get_variables()
        return [(v, dd.get_order().get_comparison(v)) for v in variables]

    comps = dict(get_comparisons(dd))

    def insert_comparisons(match):
        """Replace numeric comparison labels with human-readable strings.

        Args:
            match: The regex match object containing pre, num, and post groups.

        Returns:
            The matched string with the label replaced by the comparison description.
        """
        pre, num, pos = match.group(1, 2, 3)
        return pre + str(comps[int(num)]) + pos

    def insert_varnames(match):
        """Replace variable IDs with human-readable names.

        Args:
            match: The regex match object containing the variable ID.

        Returns:
            The variable name from to_varname, or the original match.
        """
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
