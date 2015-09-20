#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"

#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) (((nb) + CHAR_BIT - 1) / CHAR_BIT)

enum regex_token_type {
    TYPE_ATOM,
    TYPE_CONCAT,
    TYPE_ALTERNATIVE,
    TYPE_ZERO_MANY,
    TYPE_ONE_MANY,
    TYPE_ZERO_ONE,
};

struct regex_token {
    enum regex_token_type type;
    unsigned char atom[BITNSLOTS(256)];
    struct regex_token *next;
};

struct regex_paren {
    int nalt;
    int natom;
    struct regex_paren *prev;
};

enum state_type {
    STATE_ATOM,
    STATE_SPLIT,
    STATE_MATCH,
    STATE_MARK
};

struct state {
    enum state_type type;
    union {
        unsigned char atom[BITNSLOTS(256)];
    };
    struct state *o1, *o2;
};

struct ptrlist {
    struct state **s;
    struct ptrlist *next;
};

struct frag {
    struct state *start;
    struct ptrlist *out;
    struct frag *prev;
};

struct regex {
    struct state *entry;
};

struct state_list {
    struct state *s;
    struct state_list *next;
};

struct state matchstate = { STATE_MATCH };

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

struct state *state(enum state_type type, struct state *o1, struct state *o2) {
    struct state *s = malloc(sizeof(*s));
    s->type = type;
    s->o1 = o1;
    s->o2 = o2;
    return s;
}

void state_mark_recursive(struct state *s) {
    if (!s || s == &matchstate) {
        return;
    }

    s->type = STATE_MARK;

    if (s->o1 && s->o1->type == STATE_MARK) {
        s->o1 = NULL;
    }

    if (s->o2 && s->o2->type == STATE_MARK) {
        s->o2 = NULL;
    }

    state_mark_recursive(s->o1);
    state_mark_recursive(s->o2);
}

void state_free_recursive(struct state *s) {
    if (!s || s == &matchstate) {
        return;
    }

    state_free_recursive(s->o1);
    state_free_recursive(s->o2);
    free(s);
}

void state_free(struct state *s) {
    state_mark_recursive(s);

    state_free_recursive(s);
}

struct ptrlist *ptrlist_alloc(struct state **s) {
    struct ptrlist *l = malloc(sizeof(*l));
    l->s = s;
    l->next = NULL;
    return l;
}

void ptrlist_patch(struct ptrlist *l, struct state *s) {
    struct ptrlist *next;
    for (struct ptrlist *i = l; i; i = next) {
        next = i->next;
        *i->s = s;
        free(i);
    }
}

struct ptrlist *ptrlist_concat(struct ptrlist *l1, struct ptrlist *l2) {
    struct ptrlist *oldl1 = l1;

    while (l1->next) {
        l1 = l1->next;
    }
    l1->next = l2;
    return oldl1;
}

struct frag *frag(struct state *start, struct ptrlist *out, struct frag *prev) {
    struct frag *frag = malloc(sizeof(*frag));
    frag->start = start;
    frag->out = out;
    frag->prev = prev;
    return frag;
}

void frag_push(struct frag **stackp, struct state *start, struct ptrlist *out) {
    *stackp = frag(start, out, *stackp);
}

struct frag frag_pop(struct frag **stackp) {
    struct frag frag = **stackp;
    free(*stackp);
    *stackp = frag.prev;
    frag.prev = NULL;
    return frag;
}

struct state *token2nfa(struct regex_token *token) {
    if (!token) {
        return NULL;
    }

    struct frag *stack = NULL,
                e1, e2;
    struct state *s;

    for (; token; token = token->next) {
        switch (token->type) {
        case TYPE_ATOM:
            s = state(STATE_ATOM, NULL, NULL);
            memcpy(s->atom, token->atom, BITNSLOTS(256));
            frag_push(&stack, s, ptrlist_alloc(&s->o1));
            break;
        case TYPE_CONCAT:
            e2 = frag_pop(&stack);
            e1 = frag_pop(&stack);
            ptrlist_patch(e1.out, e2.start);
            frag_push(&stack, e1.start, e2.out);
            break;
        case TYPE_ALTERNATIVE:
            e2 = frag_pop(&stack);
            e1 = frag_pop(&stack);
            s = state(STATE_SPLIT, e1.start, e2.start);
            frag_push(&stack, s, ptrlist_concat(e1.out, e2.out));
            break;
        case TYPE_ZERO_MANY:
            e1 = frag_pop(&stack);
            s = state(STATE_SPLIT, e1.start, NULL);
            ptrlist_patch(e1.out, s);
            frag_push(&stack, s, ptrlist_alloc(&s->o2));
            break;
        case TYPE_ONE_MANY:
            e1 = frag_pop(&stack);
            s = state(STATE_SPLIT, e1.start, NULL);
            ptrlist_patch(e1.out, s);
            frag_push(&stack, e1.start, ptrlist_alloc(&s->o2));
            break;
        case TYPE_ZERO_ONE:
            e1 = frag_pop(&stack);
            s = state(STATE_SPLIT, e1.start, NULL);
            frag_push(&stack, s, ptrlist_concat(e1.out, ptrlist_alloc(&s->o2)));
            break;
        }
    }

    e1 = frag_pop(&stack);
    ptrlist_patch(e1.out, &matchstate);

    if (stack) {
        state_free(e1.start);
        return NULL;
    }

    return e1.start;
}

regex_t *regex_compile(char const *pattern) {
    struct regex_token *token = tokenise(pattern);
    if (!token) {
        return NULL;
    }

    struct state *state = token2nfa(token);
    token_free(token);

    if (!state) {
        return NULL;
    }

    regex_t *re = malloc(sizeof(*re));
    re->entry = state;
    return re;
}

void regex_free(regex_t *re) {
    state_free(re->entry);
    free(re);
}

void state_list_prepend(struct state_list **l, struct state *s) {
    struct state_list *nl = malloc(sizeof(*nl));
    nl->s = s;
    nl->next = *l;
    *l = nl;
}

void state_list_free(struct state_list *clist) {
    while (clist) {
        struct state_list *next = clist->next;
        free(clist);
        clist = next;
    }
}

void state_list_add(struct state_list **l, struct state *s) {
    if (!s) {
        return;
    }

    if (s->type == STATE_SPLIT) {
        state_list_add(l, s->o1);
        state_list_add(l, s->o2);
        return;
    }

    state_list_prepend(l, s);
}

void step(struct state_list *clist, int c, struct state_list **nlist) {
    for (; clist; clist = clist->next) {
        struct state *s = clist->s;
        if (s->type == STATE_ATOM && BITTEST(s->atom, c)) {
            state_list_add(nlist, s->o1);
        }
    }
}

int regex_match(regex_t *re, char const *s) {
    struct state_list *clist = NULL;
    state_list_add(&clist, re->entry);

    for (; *s; ++s) {
        struct state_list *nlist = NULL;
        step(clist, *s, &nlist);
        state_list_free(clist);
        clist = nlist;
    }

    for (struct state_list *search = clist; search; search = search->next) {
        if (search->s->type == STATE_MATCH) {
            state_list_free(clist);
            return 1;
        }
    }

    state_list_free(clist);

    return 0;
}

/* vim: set sw=4 et: */
