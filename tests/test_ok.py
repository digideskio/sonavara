import codecs
import os
import tempfile
from subprocess import PIPE
from subprocess import Popen
from subprocess import TimeoutExpired

from sonavara.main import compile


class SonavaraLexer:
    def __init__(self, *, code, context=False):
        self.code = code
        self.context = context

    def __enter__(self):
        self.compile()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.cleanup()

    def compile(self):
        f, name = tempfile.mkstemp()
        self.name = name
        os.close(f)

        p = Popen(['gcc', '-DSONAVARA_INCLUDE_FILE', '-DSONAVARA_NO_SELF_CHAIN', '-o', self.name, '-Wall', '-g', '-x', 'c', '-'], stdin=PIPE)
        compile(self.code, codecs.getwriter('utf8')(p.stdin))
        p.stdin.write(b"""
            int main(int argc, char **argv) {
                struct lexer *lexer = lexer_start_file(stdin);
""")
        if self.context:
            p.stdin.write(b"""struct lexer_context context;\n""")

        p.stdin.write(b"""

                while (1) {
""")
        if self.context:
            p.stdin.write(b"int t = lexer_lex(lexer, &context);\n")
        else:
            p.stdin.write(b"int t = lexer_lex(lexer);\n")

        p.stdin.write(b"""
                    if (t == -1) {
                        lexer_free(lexer);
                        return 1;
                    }

                    if (!t) {
                        lexer_free(lexer);
                        return 0;
                    }
                    printf("token: %d\\n", t);
""")

        if self.context:
            p.stdin.write(b"""
                    handle_token(t, &context);
""")

        p.stdin.write(b"""
                }
            }
        """)
        p.stdin.close()
        assert p.wait() == 0

    def cleanup(self):
        if self.name:
            try:
                os.unlink(self.name)
            except OSError:
                pass

    def test(self, input, tokens, error=False):
        p = Popen([self.name], stdin=PIPE, stdout=PIPE, stderr=PIPE)
        try:
            out, errs = p.communicate(input.encode('utf8'), timeout=10)
        except TimeoutExpired:
            p.kill()
            out, errs = p.communicate()

        assert p.returncode == (1 if error else 0)
        assert out == "".join(
            "token: {}\n".format(token) if isinstance(token, int) else "{}\n".format(token)
            for token in tokens
        ).encode('utf8')
        assert errs == b""


def test_base():
    with SonavaraLexer(code="""
abc
    return 1;

def
    return 2;
""") as sv:
        sv.test("abc", [1])
        sv.test("ab", [], True)
        sv.test("defabc", [2, 1])
        sv.test("defab", [2], True)


def test_context():
    with SonavaraLexer(context=True, code="""
*raw
    #include <stdio.h>
    #include <stdlib.h>

    struct lexer_context {
        char *saved;
    };

    void handle_token(int id, struct lexer_context *context) {
        if (id == 1) {
            printf("got %s\\n", context->saved);
            free(context->saved);
        }
    }

*set context = lexer_context

abc
    context->saved = strdup(match);
    return 1;

def
    return 2;

*#x
""") as sv:
        sv.test("abc", [1, "got abc"])
        sv.test("ab", [], True)
        sv.test("defabc", [2, 1, "got abc"])
        sv.test("defab", [2], True)
