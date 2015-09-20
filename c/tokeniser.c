#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tokeniser.h"

struct paren {
    int nalt;
    int natom;
    char const *last;
    struct paren *prev;
};

void token_append(struct regex_token ***writep, enum regex_token_type type) {
    **writep = malloc(sizeof(***writep));
    (**writep)->type = type;
    (**writep)->next = NULL;
    *writep = &((**writep)->next);
}

void token_append_atom(struct regex_token ***writep, unsigned char *atom) {
    **writep = malloc(sizeof(***writep));
    (**writep)->type = TYPE_ATOM;
    memcpy((**writep)->atom, atom, BITNSLOTS(256));
    (**writep)->next = NULL;
    *writep = &((**writep)->next);
}

void token_free(struct regex_token *token) {
    while (token) {
        struct regex_token *next = token->next;
        free(token);
        token = next;
    }
}

void paren_free(struct paren *paren) {
    while (paren) {
        struct paren *prev = paren->prev;
        free(paren);
        paren = prev;
    }
}

int process_escape(int v) {
    switch (v) {
    case '0': v = '\0'; break;
    case 'a': v = '\a'; break;
    case 'b': v = '\b'; break;
    case 'f': v = '\f'; break;
    case 'n': v = '\n'; break;
    case 'r': v = '\r'; break;
    case 't': v = '\t'; break;
    case 'v': v = '\v'; break;
    }
    return v;
}

enum tokeniser_state {
    DEFAULT,
    BRACE_PRE_COMMA,
    BRACE_POST_COMMA,
    BRACE_CCLASS_SUBTRACT,
    ESCAPE,
    CCLASS_START,
    CCLASS_MID,
    CCLASS_RANGE,
    CCLASS_ESCAPE,
    CCLASS_POST,
};

struct tokeniser {
    enum tokeniser_state state;
    struct regex_token **write;

    struct paren *paren;
    int natom;
    int nalt;
    char const *last;

    char const *brace_start;
    int brace_low;
    int brace_high;

    int cclass_negated;
    int cclass_last;
    int cclass_subtract;
    unsigned char cclass_atom[BITNSLOTS(256)];
    unsigned char cclass_atom_parent[BITNSLOTS(256)];
};

int process(struct tokeniser *sp, char const *pattern, char const *stop);
int tokenise_default(struct tokeniser *sp, char const *pattern);
int tokenise_escape(struct tokeniser *sp, int v);
int tokenise_brace_pre_comma(struct tokeniser *sp, int v);
int tokenise_brace_post_comma(struct tokeniser *sp, int v);
int tokenise_cclass_start(struct tokeniser *sp, int v);
int tokenise_cclass_mid(struct tokeniser *sp, int v);
int tokenise_cclass_range(struct tokeniser *sp, int v);
int tokenise_cclass_post(struct tokeniser *sp, char const **pattern);
void cclass_post_cleanup(struct tokeniser *sp);

struct regex_token *tokenise(char const *pattern) {
    struct regex_token *r = NULL;

    struct tokeniser s;
    memset(&s, 0, sizeof(s));

    s.state = DEFAULT;
    s.write = &r;

    if (!process(&s, pattern, NULL)) {
        paren_free(s.paren);
        token_free(r);
        return NULL;
    }

    if (s.paren) {
        paren_free(s.paren);
        token_free(r);
        return NULL;
    }

    if (s.state == CCLASS_POST) {
        cclass_post_cleanup(&s);
    }

    if (s.state != DEFAULT) {
        token_free(r);
        return NULL;
    }

    while (--s.natom > 0) {
        token_append(&s.write, TYPE_CONCAT);
    }

    for (; s.nalt > 0; --s.nalt) {
        token_append(&s.write, TYPE_ALTERNATIVE);
    }

    return r;
}

int process(struct tokeniser *sp, char const *pattern, char const *stop) {
    unsigned char atom[BITNSLOTS(256)];


    for (; pattern != stop && *pattern; ++pattern) {
        int abort = 0,
            v = *pattern;

        switch (sp->state) {
        case DEFAULT:
            abort = !tokenise_default(sp, pattern);
            break;

        case BRACE_PRE_COMMA:
            abort = !tokenise_brace_pre_comma(sp, v);
            break;

        case BRACE_POST_COMMA:
            abort = !tokenise_brace_post_comma(sp, v);
            break;

        case BRACE_CCLASS_SUBTRACT:
            if (v != '[') {
                abort = 1;
                break;
            }

            memcpy(sp->cclass_atom_parent, sp->cclass_atom, BITNSLOTS(256));
            abort = !tokenise_default(sp, pattern);
            sp->cclass_subtract = 1;

            break;

        case ESCAPE:
            abort = !tokenise_escape(sp, v);
            break;

        case CCLASS_START:
            abort = !tokenise_cclass_start(sp, v);
            break;

        case CCLASS_MID:
            abort = !tokenise_cclass_mid(sp, v);
            break;

        case CCLASS_RANGE:
            abort = !tokenise_cclass_range(sp, v);
            break;

        case CCLASS_ESCAPE:
            sp->state = CCLASS_MID;
            v = process_escape(v);
            BITSET(sp->cclass_atom, v);
            break;

        case CCLASS_POST:
            abort = !tokenise_cclass_post(sp, &pattern);
            break;
        }

        if (abort) {
            return 0;
        }
    }

    return 1;
}

int tokenise_default(struct tokeniser *sp, char const *pattern) {
    unsigned char atom[BITNSLOTS(256)];

    switch (*pattern) {
    case '{':
        sp->state = BRACE_PRE_COMMA;
        sp->brace_low = -1;
        sp->brace_high = -1;
        sp->brace_start = pattern;
        break;

    case '\\':
        sp->state = ESCAPE;
        sp->last = pattern;
        break;

    case '[':
        sp->state = CCLASS_START;
        sp->cclass_negated = 0;
        sp->cclass_last = 0;
        sp->cclass_subtract = 0;
        memset(sp->cclass_atom, 0, BITNSLOTS(256));
        sp->last = pattern;
        break;

    case '(':
        if (sp->natom > 1) {
            --sp->natom;
            token_append(&sp->write, TYPE_CONCAT);
        }

        struct paren *new_paren = malloc(sizeof(*new_paren));
        new_paren->nalt = sp->nalt;
        new_paren->natom = sp->natom;
        new_paren->last = pattern;
        new_paren->prev = sp->paren;
        sp->paren = new_paren;

        sp->nalt = 0;
        sp->natom = 0;
        sp->last = 0;
        break;

    case ')':
        if (!sp->paren || sp->natom == 0) {
            return 0;
        }

        while (--sp->natom > 0) {
            token_append(&sp->write, TYPE_CONCAT);
        }
        
        for (; sp->nalt > 0; --sp->nalt) {
            token_append(&sp->write, TYPE_ALTERNATIVE);
        }

        sp->nalt = sp->paren->nalt;
        sp->natom = sp->paren->natom;
        sp->last = sp->paren->last;
        struct paren *old_paren = sp->paren->prev;
        free(sp->paren);
        sp->paren = old_paren;

        ++sp->natom;
        break;


    case '|':
        if (sp->natom == 0) {
            return 0;
        }
        while (--sp->natom > 0) {
            token_append(&sp->write, TYPE_CONCAT);
        }
        ++sp->nalt;
        sp->last = 0;
        break;

    case '*':
        if (sp->natom == 0) {
            return 0;
        }
        token_append(&sp->write, TYPE_ZERO_MANY);
        sp->last = 0;
        break;

    case '+':
        if (sp->natom == 0) {
            return 0;
        }
        token_append(&sp->write, TYPE_ONE_MANY);
        sp->last = 0;
        break;

    case '?':
        if (sp->natom == 0) {
            return 0;
        }
        token_append(&sp->write, TYPE_ZERO_ONE);
        sp->last = 0;
        break;

    case '.':
        if (sp->natom > 1) {
            --sp->natom;
            token_append(&sp->write, TYPE_CONCAT);
        }
        memset(atom, 0xff, BITNSLOTS(256));
        BITCLEAR(atom, '\n');
        token_append_atom(&sp->write, atom);
        ++sp->natom;
        sp->last = pattern;
        break;

    default:
        if (sp->natom > 1) {
            --sp->natom;
            token_append(&sp->write, TYPE_CONCAT);
        }
        memset(atom, 0, BITNSLOTS(256));
        BITSET(atom, *pattern);
        token_append_atom(&sp->write, atom);
        ++sp->natom;
        sp->last = pattern;
        break;
    }

    return 1;
}

int tokenise_escape(struct tokeniser *sp, int v) {
    sp->state = DEFAULT;

    if (sp->natom > 1) {
        --sp->natom;
        token_append(&sp->write, TYPE_CONCAT);
    }

    unsigned char atom[256];

    memset(atom, 0, BITNSLOTS(256));
    v = process_escape(v);
    BITSET(atom, v);

    token_append_atom(&sp->write, atom);
    ++sp->natom;

    return 1;
}

int tokenise_brace_pre_comma(struct tokeniser *sp, int v) {
    if (v == ',') {
        sp->state = BRACE_POST_COMMA;
        return 1;
    }

    if (v == '}') {
        if (sp->brace_low == -1) {
            return 0;
        }

        sp->state = DEFAULT;

        char const *last = sp->last,
             *brace_start = sp->brace_start;
        int brace_low = sp->brace_low;

        for (int i = 0; i < brace_low - 1; ++i) {
            if (!process(sp, last, brace_start)) {
                return 0;
            }
        }

        return 1;
    }

    if (!isdigit(v)) {
        return 0;
    }

    if (sp->brace_low == -1) {
        sp->brace_low = 0;
    }

    sp->brace_low = (sp->brace_low * 10) + (v - '0');
    return 1;
}

int tokenise_brace_post_comma(struct tokeniser *sp, int v) {
    if (v == '}') {
        sp->state = DEFAULT;

        char const *last = sp->last,
             *brace_start = sp->brace_start;

        int brace_low = sp->brace_low,
            brace_high = sp->brace_high;

        // Note that one instance of the to-be-repeated content is already on
        // the token stream.

        if (brace_low < 1 && brace_high == -1) {
            tokenise_default(sp, "*");
        } else if (brace_low == 1 && brace_high == -1) {
            tokenise_default(sp, "+");
        } else if (brace_high == -1) {
            for (int i = 1; i < brace_low; ++i) {
                process(sp, last, brace_start);
            }
            tokenise_default(sp, "+");
        } else {
            if (brace_low < 1) {
                tokenise_default(sp, "?");
                --brace_high;
            }

            for (int i = 1; i < brace_low; ++i) {
                if (!process(sp, last, brace_start)) {
                    return 0;
                }
            }

            for (int i = brace_low; i < brace_high; ++i) {
                process(sp, last, brace_start);
                tokenise_default(sp, "?");
            }
        }

        return 1;
    }

    if (!isdigit(v)) {
        return 0;
    }


    if (sp->brace_high == -1) {
        sp->brace_high = 0;
    }

    sp->brace_high = (sp->brace_high * 10) + (v - '0');
    return 1;
}

int tokenise_cclass_start(struct tokeniser *sp, int v) {
    sp->state = CCLASS_MID;

    if (v == '^') {
        sp->cclass_negated = 1;
        return 1;
    }

    return tokenise_cclass_mid(sp, v);
}

int tokenise_cclass_mid(struct tokeniser *sp, int v) {
    switch (v) {
    case '\\':
        sp->state = CCLASS_ESCAPE;
        break;

    case ']':
        sp->state = CCLASS_POST;

        if (sp->cclass_negated) {
            for (int i = 0; i < BITNSLOTS(256); ++i) {
                sp->cclass_atom[i] = ~sp->cclass_atom[i];
            }
        }

        if (sp->cclass_subtract) {
            for (int i = 0; i < 256; ++i) {
                if (BITTEST(sp->cclass_atom, i)) {
                    BITCLEAR(sp->cclass_atom_parent, i);
                }
            }
            memcpy(sp->cclass_atom, sp->cclass_atom_parent, BITNSLOTS(256));
        }
        break;

    case '-':
        if (!sp->cclass_last) {
            sp->cclass_last = v;
            BITSET(sp->cclass_atom, sp->cclass_last);
        } else {
            sp->state = CCLASS_RANGE;
        }
        break;

    default:
        sp->cclass_last = v;
        BITSET(sp->cclass_atom, v);
        break;
    }

    return 1;
}

int tokenise_cclass_range(struct tokeniser *sp, int v) {
    if (v == ']') {
        BITSET(sp->cclass_atom, v);
        return tokenise_cclass_mid(sp, v);
    }

    for (int i = sp->cclass_last; i <= v; ++i) {
        BITSET(sp->cclass_atom, i);
    }

    sp->cclass_last = 0;
    sp->state = CCLASS_MID;
    return 1;
}

int tokenise_cclass_post(struct tokeniser *sp, char const **pattern) {
    if (strncmp(*pattern, "{-}", 3) == 0) {
        *pattern += 2;
        sp->state = BRACE_CCLASS_SUBTRACT;
        return 1;
    }

    cclass_post_cleanup(sp);

    return tokenise_default(sp, *pattern);
}

void cclass_post_cleanup(struct tokeniser *sp) {
    if (sp->natom > 1) {
        --sp->natom;
        token_append(&sp->write, TYPE_CONCAT);
    }

    token_append_atom(&sp->write, sp->cclass_atom);
    ++sp->natom;
    sp->state = DEFAULT;
}

/* vim: set sw=4 et: */
