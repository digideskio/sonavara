#include <stdio.h>
#include <stdlib.h>

#include "engine.h"

int identifier(char const *match) {
    return 1;
}

int equals(char const *match) {
    return 2;
}

int number(char const *match) {
    return 3;
}

int plus(char const *match) {
    return 4;
}

struct lexer_rule {
    char const *pattern;
    int (*action)(char const *match);
    struct regex *re;
} rules[] = {
    {"[[:alpha:]][[:alnum:]_]*", identifier},
    {"=", equals},
    {"[[:digit:]]+", number},
    {"\\+", plus},
    {"[[:space:]]+", NULL},
    {NULL, NULL},
};

struct lexer {
    char const *src;
};

struct lexer *lexer_start(char const *src) {
    for (struct lexer_rule *rule = rules; rule->pattern; ++rule) {
        if (rule->re) {
            break;
        }

        rule->re = regex_compile(rule->pattern);
        if (!rule->re) {
            fprintf(stderr, "failed to compile: %s\n", rule->pattern);
            exit(1);
        }
    }

    struct lexer *lexer = malloc(sizeof(*lexer));
    lexer->src = src;
    return lexer;
}

int lexer_lex(struct lexer *lexer) {
start:
    for (struct lexer_rule *rule = rules; rule->pattern; ++rule) {
        int len = regex_match_prefix(rule->re, lexer->src);
        if (len <= 0) {
            continue;
        }

        lexer->src += len;
        if (!rule->action) {
            goto start;
        }

        return rule->action(lexer->src - len);
    }

    return 0;
}

void lexer_free(struct lexer *lexer) {
    free(lexer);
}

int main(int argc, char **argv) {
    struct lexer *lexer = lexer_start("a = 1 + 2");

    while (1) {
        int t = lexer_lex(lexer);
        if (!t) {
            printf("EOF\n");
            break;
        }

        printf("token: %d\n", t);
    }

    lexer_free(lexer);

    return 0;
}

/* vim: set sw=4 et: */
