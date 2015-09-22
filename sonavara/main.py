import sys
from pkg_resources import resource_string


def compile(input):
    print(resource_string('c', 'lexer.c'))


def main():
    return 0


if __name__ == '__main__':
    sys.exit(main())
