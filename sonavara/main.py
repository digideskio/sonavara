import io
import sys
from pkg_resources import resource_string


__all__ = ['compile']


def write_prelude(output):
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


def parse(input):
    return []


def compile(input, output=None):
    write_prelude(output)
    fns = parse(input)

    for i, (pattern, body) in enumerate(fns):
        output.write(b"static int lexer_fn_")
        output.write(str(i).encode('ascii'))
        output.write(b"(char *match) {\n")
        output.write(body)
        output.write(b"\n}\n")

    output.write(b"struct lexer_rule rules[] = {\n")
    for i, (pattern, body) in enumerate(fns):
        output.write(b"    {\"")
        output.write(pattern)
        output.write(b"\", lexer_fn_")
        output.write(str(i).encode('ascii'))
        output.write(b"},\n")

    output.write(b"    {NULL, NULL},\n")
    output.write(b"};\n")

    if isinstance(output, io.BytesIO):
        v = output.getvalue()
        output.close()
        return v


def main():
    return 0


if __name__ == '__main__':
    sys.exit(main())
