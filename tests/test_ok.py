import subprocess

from sonavara.main import compile


class SonavaraLexer:
    def __init__(self, code):
        pass

    def test(self, input, tokens, error=False):
        p = subprocess.Popen(['gcc', '-DNO_SELF_CHAIN', '-o', '/tmp/a.out', '-Wall', '-g', '-x', 'c', '-'], stdin=subprocess.PIPE)
        compile(input, p.stdin)
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

        p = subprocess.Popen(['/tmp/a.out'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            out, errs = p.communicate(input, timeout=10)
        except subprocess.TimeoutExpired:
            p.kill()
            out, errs = p.communicate()

        assert errs == b""


def test_thing():
    sv = SonavaraLexer("""abc           return 1;""")
    sv.test("abc", [1])
    sv.test("ab", [], True)
