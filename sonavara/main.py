import io
import re
import sys
from pkg_resources import resource_string


__all__ = ['compile']


def write_prelude(output, context):
    sources = [
        'tokeniser.c',
        'nfa.c',
        'engine.c',
        'lexer.c',
    ]

    if output is None:
        output = io.StringIO()

    for filename in sources:
        output.write(resource_string('sonavara.c', filename).decode('utf8'))

    output.write("\n")

    output.write("int lexer_lex(struct lexer *lexer{}) {{\n".format(", {} *context".format(context) if context else ""))
    output.write("""
start:
    if (*lexer->src == 0) {
        return 0;
    }

    for (struct lexer_rule *rule = rules; rule->pattern; ++rule) {
        int len = regex_match_prefix(rule->re, lexer->src);
        if (len <= 0) {
            continue;
        }

        lexer->src += len;
        if (!rule->action) {
            goto start;
        }

        char *match = strndup(lexer->src - len, len);
        int skip = 0;
""")
    output.write("int token = rule->action(match, {}, &skip);\n".format("context" if context else "NULL"))
    output.write("""
        free(match);

        if (skip) {
            goto start;
        }

        return token;
    }

    return -1;
}
    """)

    output.write("\n")


class Parser:
    def resolve_current_action(self):
        if not self.current_action:
            return

        self.result['fns'].append((self.current_action, self.current_action_io.getvalue()))
        self.current_action_io.close()
        self.current_action = None
        self.current_action_io = None

    def handle_base(self, line):
        if line.strip() == '':
            return

        if re.match(r'^\s', line):
            raise ValueError("Found action {} in base state".format(repr(line)))

        option = re.match('^\*set\s+(\w+)\s*=(.+)$', line)
        if option:
            key, value = option.groups()
            value = value.strip()

            if key == 'context':
                self.result['context'] = value
            else:
                raise ValueError(key)
            return

        if line.strip() == '*raw':
            self.state = 'raw'
            return

        if line.startswith('*#'):
            return

        self.state = 'action'
        self.current_action = line.rstrip('\n')
        self.current_action_io = io.StringIO()
        while self.current_action[-1] == ' ' and self.current_action[-2] != "\\":
            self.current_action = self.current_action[:-1]

    def handle_raw(self, line):
        if line.strip() == '':
            self.result['raw'] += "\n"
            return

        if line.strip() and not re.match(r'^\s', line):
            self.action = 'base'
            return self.handle_base(line)

        self.result['raw'] += line

    def handle_action(self, line):
        if line.strip() == '':
            self.current_action_io.write("\n")
            return

        if not re.match(r'^\s', line):
            self.resolve_current_action()
            self.state = 'base'
            return self.handle_base(line)

        self.current_action_io.write(line)

    def parse(self, input):
        self.state = 'base'
        self.current_action = None
        self.current_action_io = None

        self.result = {
            'fns': [],
            'raw': '',
        }

        for line in io.StringIO(input):
            if self.state == 'base':
                self.handle_base(line)
            elif self.state == 'raw':
                self.handle_raw(line)
            elif self.state == 'action':
                self.handle_action(line)

        self.resolve_current_action()

        return self.result


def compile(input, output=None):
    parsed = Parser().parse(input)
    output.write(parsed['raw'])
    write_prelude(output, parsed.get('context'))

    for i, (pattern, body) in enumerate(parsed['fns']):
        output.write("#pragma GCC diagnostic push\n")
        output.write("#pragma GCC diagnostic ignored \"-Wunused-variable\"\n")
        output.write("static int lexer_fn_{}(char *match, void *_context, int *_skip) {{\n".format(i))
        if 'context' in parsed:
            output.write("    {} *context = _context;\n".format(parsed['context']))
        output.write(body)
        output.write("\n")
        output.write("    *_skip = 1;\n")
        output.write("    return 0;\n")
        output.write("}\n")
        output.write("#pragma GCC diagnostic pop\n")

    output.write("struct lexer_rule rules[] = {\n")
    for i, (pattern, body) in enumerate(parsed['fns']):
        output.write("    {{\"{}\", lexer_fn_{}}},\n".format(pattern, i))

    output.write("    {NULL, NULL},\n")
    output.write("};\n")

    if isinstance(output, io.StringIO):
        v = output.getvalue()
        output.close()
        return v


def main():
    compile(sys.stdin.read(), sys.stdout)


if __name__ == '__main__':
    sys.exit(main())
