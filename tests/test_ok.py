import os
import subprocess
import tempfile

from sonavara.main import compile


class SonavaraLexer:
    def __init__(self, code):
        self.code = code
        self.compile()

    def compile(self):
        f, name = tempfile.mkstemp()
        self.name = name
        os.close(f)

        p = subprocess.Popen(['gcc', '-DNO_SELF_CHAIN', '-o', self.name, '-Wall', '-g', '-x', 'c', '-'], stdin=subprocess.PIPE)
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
        os.unlink(self.name)

    def test(self, input, tokens, error=False):
        p = subprocess.Popen([self.name], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            out, errs = p.communicate(input.encode('utf8'), timeout=10)
        except subprocess.TimeoutExpired:
            p.kill()
            out, errs = p.communicate()

        assert p.returncode == (1 if error else 0)
        assert out == "".join("token: {}\n".format(token) for token in tokens).encode('utf8')
        assert errs == b""


def test_thing():
    sv = SonavaraLexer("""abc           return 1;""")
    try:
        sv.test("abc", [1])
        sv.test("ab", [], True)
    finally:
        sv.cleanup()
