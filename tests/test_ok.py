import subprocess

from sonavara.main import compile


class SonavaraLexer:
    def __init__(self, code):
        pass

    def test(self, input, tokens, error=False):
        with subprocess.Popen(['gcc', '-DNO_SELF_CHAIN', '-o', '/tmp/a.out', '-Wall', '-g', '-x', 'c', '-'], stdin=subprocess.PIPE) as p:
            compile(input, p.stdin)
        import sys
        sys.exit(1)
        # subprocess.run
        # pass


def test_thing():
    sv = SonavaraLexer("""abc           return 1;""")
    sv.test("abc", [1])
    sv.test("ab", [], True)
