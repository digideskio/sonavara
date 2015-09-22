import io
import sys
from pkg_resources import resource_string


def compile(input, output=None):
    sources = [
        'tokeniser.c',
        'nfa.c',
        'engine.c',
        'lexer.c',
    ]

    if output is None:
        output = io.BytesIO()

    for filename in sources:
        output.write(resource_string('c', filename))

    output.write(b"\n")

    if isinstance(output, io.BytesIO):
        v = output.getvalue()
        output.close()
        return v


def main():
    return 0


if __name__ == '__main__':
    sys.exit(main())
