class SonavaraLexer:
    def __init__(self, code):
        pass

    def test(self, input, tokens, error=False):
        pass


def test_thing():
    sv = SonavaraLexer("""abc           return 1;""")
    sv.test("abc", [1])
    sv.test("ab", [], True)
