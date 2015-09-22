#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NO_SELF_CHAIN
#include "engine.c"
#endif

struct lexer_rule {
    char const *pattern;
    int (*action)(char *match);

    struct regex *re;
};

extern struct lexer_rule rules[];

struct lexer {
    char const *src;
    char *buffer;
};

static void lexer_init(void) {
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
}

struct lexer *lexer_start_str(char const *src) {
    lexer_init();

    struct lexer *lexer = malloc(sizeof(*lexer));
    lexer->src = src;
    lexer->buffer = NULL;
    return lexer;
}

struct lexer *lexer_start_file(FILE *file) {
    int buffersz = 2;
    int n = 0;
    char *buffer = malloc(buffersz);

    while (!feof(file)) {
        if (n == buffersz - 1) {
            buffersz *= 2;
            char *newbuffer = malloc(buffersz);
            memcpy(newbuffer, buffer, n);
            free(buffer);
            buffer = newbuffer;
        }

        size_t r = fread(buffer + n, 1, buffersz - n - 1, file);

        if (r <= 0) {
            break;
        }

        n += r;
    }

    buffer[n] = 0;

    struct lexer *lexer = lexer_start_str(buffer);
    lexer->buffer = buffer;
    return lexer;
}


int lexer_lex(struct lexer *lexer) {
start:
    if (*lexer->src == 0) {
        return 0;
    }

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

    return -1;
}

void lexer_free(struct lexer *lexer) {
    free(lexer->buffer);
    free(lexer);
}

/* vim: set sw=4 et: */
