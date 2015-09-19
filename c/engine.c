#include <stdio.h>
#include <stdlib.h>

struct partial_nfa {
    int c;
    struct partial_nfa *o1, *o2;
};

typedef struct {
    struct partial_nfa *entry;
} regex_t;

enum regex_token_type {
    ATOM,
    CONCAT,
    ALTERNATIVE,
    ZERO_MANY,
    ONE_MANY,
    ZERO_ONE,
};

struct regex_token {
    enum regex_token_type type;
    union {
        char atom;
    };
    struct regex_token *next;
};

void append_token(struct regex_token ***writep, enum regex_token_type type) {
    **writep = malloc(sizeof(***writep));
    (**writep)->type = type;
    (**writep)->next = NULL;
    *writep = &((**writep)->next);
}

void append_atom(struct regex_token ***writep, char atom) {
    **writep = malloc(sizeof(***writep));
    (**writep)->type = ATOM;
    (**writep)->atom = atom;
    (**writep)->next = NULL;
    *writep = &((**writep)->next);
}

void free_token(struct regex_token *token) {
    while (token) {
        struct regex_token *next = token->next;
        free(token);
        token = next;
    }
}

struct regex_paren {
    int nalt;
    int natom;
    struct regex_paren *prev;
};

void free_parens(struct regex_paren *paren) {
    while (paren) {
        struct regex_paren *prev = paren->prev;
        free(paren);
        paren = prev;
    }
}

struct regex_token *tokenise(char const *pattern) {
    struct regex_token *r = NULL,
                 **write = &r;

    struct regex_paren *paren = NULL;
    int natom = 0;
    int nalt = 0;

    for (; *pattern; ++pattern) {
        switch (*pattern) {
        case '(':
            if (natom > 1) {
                --natom;
                append_token(&write, CONCAT);
            }

            struct regex_paren *new_paren = malloc(sizeof(*new_paren));
            new_paren->nalt = nalt;
            new_paren->natom = natom;
            new_paren->prev = paren;
            paren = new_paren;

            nalt = 0;
            natom = 0;
            break;

        case ')':
            if (!paren || natom == 0) {
                free_token(r);
                free_parens(paren);
                return NULL;
            }

            while (--natom) {
                append_token(&write, CONCAT);
            }
            
            for (; nalt > 0; --nalt) {
                append_token(&write, ALTERNATIVE);
            }

            nalt = paren->nalt;
            natom = paren->natom;
            struct regex_paren *old_paren = paren->prev;
            free(paren);
            paren = old_paren;

            ++natom;
            break;


        case '|':
            if (natom == 0) {
                free_token(r);
                free_parens(paren);
                return NULL;
            }
            while (--natom > 0) {
                append_token(&write, CONCAT);
            }
            ++nalt;
            break;

        case '*':
            if (natom == 0) {
                free_token(r);
                free_parens(paren);
                return NULL;
            }
            append_token(&write, ZERO_MANY);
            break;

        case '+':
            if (natom == 0) {
                free_token(r);
                free_parens(paren);
                return NULL;
            }
            append_token(&write, ONE_MANY);
            break;

        case '?':
            if (natom == 0) {
                free_token(r);
                free_parens(paren);
                return NULL;
            }
            append_token(&write, ZERO_ONE);
            break;

        default:
            if (natom > 1) {
                --natom;
                append_token(&write, CONCAT);
            }
            append_atom(&write, *pattern);
            ++natom;
            break;
        }
    }

    if (paren) {
            free_parens(paren);
            free_token(r);
            return NULL;
    }

    while (--natom > 0) {
        append_token(&write, CONCAT);
    }

    for (; nalt > 0; --nalt) {
        append_token(&write, ALTERNATIVE);
    }

    return r;
}

void format_token(struct regex_token *token) {
    while (token) {
        switch (token->type) {
        case ATOM:
            printf("%c", token->atom);
            break;
        case CONCAT:
            printf(".");
            break;
        case ALTERNATIVE:
            printf("|");
            break;
        case ZERO_MANY:
            printf("*");
            break;
        case ONE_MANY:
            printf("+");
            break;
        case ZERO_ONE:
            printf("?");
            break;
        }

        token = token->next;
    }

    printf("\n");
}

regex_t *compile(char const *pattern) {
    struct regex_token *token = tokenise(pattern);
    format_token(token);
    free_token(token);

    regex_t *re = malloc(sizeof(*re));
    return re;
}

int match(regex_t *re, char const *s) {
    return 0;
}

int main(int argc, char **argv) {
    regex_t *re = compile("(ab)*d");
    printf("ababd: %d\n", match(re, "ababd"));
    printf("abd: %d\n", match(re, "abd"));
    printf("d: %d\n", match(re, "d"));
    printf("xd: %d\n", match(re, "xd"));
    return 0;
}

/* vim: set sw=4 et: */
