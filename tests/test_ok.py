import subprocess

from sonavara.main import compile


class SonavaraLexer:
    def __init__(self, code):
        pass

    def test(self, input, tokens, error=False):
        compile(input)
        import sys
        sys.exit(1)
        # subprocess.run
        # pass


def test_thing():
    sv = SonavaraLexer("""abc           return 1;""")
    sv.test("abc", [1])
    sv.test("ab", [], True)
