#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"

struct lexer_rule {
    char const *pattern;
    int (*action)(char *match);

    struct regex *re;
};

extern struct lexer_rule rules[];

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

        char *match = strndup(lexer->src - len, len);
        int token = rule->action(match);
        free(match);

        return token;
    }

    return 0;
}

void lexer_free(struct lexer *lexer) {
    free(lexer);
}

/* vim: set sw=4 et: */
