import os
import tempfile
from subprocess import PIPE
from subprocess import Popen
from subprocess import TimeoutExpired

from sonavara.main import compile


class SonavaraLexer:
    def __init__(self, code):
        self.code = code

    def __enter__(self):
        self.compile()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.cleanup()

    def compile(self):
        f, name = tempfile.mkstemp()
        self.name = name
        os.close(f)

        p = Popen(['gcc', '-DNO_SELF_CHAIN', '-o', self.name, '-Wall', '-g', '-x', 'c', '-'], stdin=PIPE)
        compile(self.code, p.stdin)
        p.stdin.write(b"""
            int main(int argc, char **argv) {
                struct lexer *lexer = lexer_start_file(stdin);

                while (1) {
                    int t = lexer_lex(lexer);
                    if (t == -1) {
                        lexer_free(lexer);
                        return 1;
                    }

                    if (!t) {
                        lexer_free(lexer);
                        return 0;
                    }

                    printf("token: %d\\n", t);
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
        assert out == "".join("token: {}\n".format(token) for token in tokens).encode('utf8')
        assert errs == b""


def test_thing():
    with SonavaraLexer("""abc           return 1;""") as sv:
        sv.test("abc", [1])
        sv.test("ab", [], True)
