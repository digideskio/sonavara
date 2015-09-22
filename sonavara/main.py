import io
import re
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
        output.write(resource_string('sonavara.c', filename))

    output.write(b"\n")


class Parser:
    def resolve_current_action(self):
        if not self.current_action:
            return

        self.result.append((self.current_action.encode('utf8'), self.current_action_io.getvalue().encode('utf8')))
        self.current_action_io.close()
        self.current_action_io = None

    def handle_base(self, line):
        if line.strip() == '':
            return

        if re.match(r'^\s', line):
            raise ValueError("Found action {} in base state".format(repr(line)))

        self.state = 'action'
        self.current_action = line.rstrip('\n')
        self.current_action_io = io.StringIO()
        self.offset = None
        while self.current_action[-1] == ' ' and self.current_action[-2] != "\\":
            self.current_action = self.current_action[:-1]

    def handle_action(self, line):
        if line.strip() == '':
            self.current_action_io.write("\n")
            return

        ws = re.match(r'^\s+', line)
        if not ws:
            self.resolve_current_action()
            self.state = 'base'
            return self.handle_base(line)

        if self.offset is None:
            self.offset = ws.group(0)
        elif not line.startswith(self.offset):
            raise ValueError("Found bad indentation in action {}".format(self.current_action))

        self.current_action_io.write(line)

    def parse(self, input):
        self.state = 'base'
        self.current_action = None
        self.current_action_io = None
        self.offset = None

        self.result = []

        for line in io.StringIO(input):
            if self.state == 'base':
                self.handle_base(line)
            elif self.state == 'action':
                self.handle_action(line)

        self.resolve_current_action()

        return self.result


def compile(input, output=None):
    write_prelude(output)
    fns = Parser().parse(input)

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
