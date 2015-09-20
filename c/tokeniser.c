#include <stdlib.h>
#include <string.h>

#include "tokeniser.h"

struct regex_paren {
    int nalt;
    int natom;
    struct regex_paren *prev;
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


void paren_free(struct regex_paren *paren) {
    while (paren) {
        struct regex_paren *prev = paren->prev;
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

struct regex_token *tokenise(char const *pattern) {
    struct regex_token *r = NULL,
                 **write = &r;

    struct regex_paren *paren = NULL;
    int natom = 0;
    int nalt = 0;
    int escape = 0;
    int cclass = 0;
    int cclass_any = 0;
    int cclass_negated = 0;
    int cclass_last = 0;
    int cclass_range = 0;
    int cclass_escape = 0;

    unsigned char atom[BITNSLOTS(256)];

    for (; *pattern; ++pattern) {
        if (cclass) {
            if (!cclass_any && *pattern == '^') {
                cclass_negated = 1;
            } else if (cclass_escape) {
                cclass_escape = 0;
                int v = process_escape(*pattern);
                BITSET(atom, v);
            } else {
                switch (*pattern) {
                case '\\':
                    cclass_escape = 1;
                    break;
                case ']':
                    if (cclass_range) {
                        BITSET(atom, (int) '-');
                    }

                    if (cclass_negated) {
                        for (int i = 0; i < BITNSLOTS(256); ++i) {
                            atom[i] = ~atom[i];
                        }
                    }
                        
                    cclass = 0;
                    if (natom > 1) {
                        --natom;
                        token_append(&write, TYPE_CONCAT);
                    }
                    token_append_atom(&write, atom);
                    ++natom;
                    break;

                case '-':
                    if (!cclass_last) {
                        cclass_last = (int) '-';
                        BITSET(atom, cclass_last);
                    } else {
                        cclass_range = 1;
                    }
                    break;

                default:
                    if (cclass_range) {
                        for (int i = cclass_last; i <= (int) *pattern; ++i) {
                            BITSET(atom, i);
                        }
                        cclass_last = (int) *pattern;
                        cclass_range = 0;
                    } else {
                        cclass_last = (int) *pattern;
                        BITSET(atom, cclass_last);
                    }
                    break;
                }
            }

            cclass_any = 1;
            continue;
        }

        if (escape) {
            escape = 0;
            if (natom > 1) {
                --natom;
                token_append(&write, TYPE_CONCAT);
            }

            memset(atom, 0, BITNSLOTS(256));
            int v = process_escape(*pattern);
            BITSET(atom, v);

            token_append_atom(&write, atom);
            ++natom;
            continue;
        }

        switch (*pattern) {
        case '\\':
            escape = 1;
            break;

        case '[':
            cclass = 1;
            cclass_any = 0;
            cclass_negated = 0;
            cclass_range = 0;
            cclass_last = 0;
            cclass_escape = 0;
            memset(atom, 0, BITNSLOTS(256));
            break;

        case '(':
            if (natom > 1) {
                --natom;
                token_append(&write, TYPE_CONCAT);
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
                token_free(r);
                paren_free(paren);
                return NULL;
            }

            while (--natom > 0) {
                token_append(&write, TYPE_CONCAT);
            }
            
            for (; nalt > 0; --nalt) {
                token_append(&write, TYPE_ALTERNATIVE);
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
                token_free(r);
                paren_free(paren);
                return NULL;
            }
            while (--natom > 0) {
                token_append(&write, TYPE_CONCAT);
            }
            ++nalt;
            break;

        case '*':
            if (natom == 0) {
                token_free(r);
                paren_free(paren);
                return NULL;
            }
            token_append(&write, TYPE_ZERO_MANY);
            break;

        case '+':
            if (natom == 0) {
                token_free(r);
                paren_free(paren);
                return NULL;
            }
            token_append(&write, TYPE_ONE_MANY);
            break;

        case '?':
            if (natom == 0) {
                token_free(r);
                paren_free(paren);
                return NULL;
            }
            token_append(&write, TYPE_ZERO_ONE);
            break;

        case '.':
            if (natom > 1) {
                --natom;
                token_append(&write, TYPE_CONCAT);
            }
            memset(atom, 0xff, BITNSLOTS(256));
            BITCLEAR(atom, '\n');
            token_append_atom(&write, atom);
            ++natom;
            break;

        default:
            if (natom > 1) {
                --natom;
                token_append(&write, TYPE_CONCAT);
            }
            memset(atom, 0, BITNSLOTS(256));
            BITSET(atom, (int) *pattern);
            token_append_atom(&write, atom);
            ++natom;
            break;
        }
    }

    if (paren) {
        paren_free(paren);
        token_free(r);
        return NULL;
    }

    if (escape || cclass) {
        token_free(r);
        return NULL;
    }

    while (--natom > 0) {
        token_append(&write, TYPE_CONCAT);
    }

    for (; nalt > 0; --nalt) {
        token_append(&write, TYPE_ALTERNATIVE);
    }

    return r;
}

