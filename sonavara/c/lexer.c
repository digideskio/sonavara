#include <stdlib.h>
#include <string.h>

#ifndef SONAVARA_NO_SELF_CHAIN
#include "engine.c"
#endif

#define BEGIN(r) current_rules = rules_##r
#define END() current_rules = rules

struct lexer_rule {
    char const *pattern;
    int (*action)(char *match, void *_context, int *_skip);

    struct regex *re;
};

extern struct lexer_rule *current_rules;
extern struct lexer_rule rules[];

struct lexer {
    char const *src;
    char *buffer;
};

static int lexer_init(struct lexer_rule *rules) {
    for (struct lexer_rule *rule = rules; rule->pattern; ++rule) {
        if (rule->re) {
            return 1;
        }

        rule->re = regex_compile(rule->pattern);
        if (!rule->re) {
            return 0;
        }
    }
    return 1;
}

static int lexer_init_all();

struct lexer *lexer_start_str(char const *src) {
    if (!lexer_init_all()) {
        return NULL;
    }

    current_rules = rules;

    struct lexer *lexer = malloc(sizeof(*lexer));
    lexer->src = src;
    lexer->buffer = NULL;
    return lexer;
}

#ifdef SONAVARA_INCLUDE_FILE

#include <stdio.h>

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
#endif


void lexer_free(struct lexer *lexer) {
    free(lexer->buffer);
    free(lexer);
}

/* vim: set sw=4 et: */
