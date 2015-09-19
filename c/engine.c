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

void append_token(struct regex_token ***writep, enum regex_token_type type) {
    **writep = malloc(sizeof(***writep));
    (**writep)->type = type;
    (**writep)->next = NULL;
    *writep = &((**writep)->next);
}

void append_atom(struct regex_token ***writep, char atom) {
    **writep = malloc(sizeof(***writep));
    (**writep)->type = TYPE_ATOM;
    memset((**writep)->atom, 0, BITNSLOTS(256));
    BITSET((**writep)->atom, (int) atom);
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
                append_token(&write, TYPE_CONCAT);
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
                append_token(&write, TYPE_CONCAT);
            }
            
            for (; nalt > 0; --nalt) {
                append_token(&write, TYPE_ALTERNATIVE);
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
                append_token(&write, TYPE_CONCAT);
            }
            ++nalt;
            break;

        case '*':
            if (natom == 0) {
                free_token(r);
                free_parens(paren);
                return NULL;
            }
            append_token(&write, TYPE_ZERO_MANY);
            break;

        case '+':
            if (natom == 0) {
                free_token(r);
                free_parens(paren);
                return NULL;
            }
            append_token(&write, TYPE_ONE_MANY);
            break;

        case '?':
            if (natom == 0) {
                free_token(r);
                free_parens(paren);
                return NULL;
            }
            append_token(&write, TYPE_ZERO_ONE);
            break;

        default:
            if (natom > 1) {
                --natom;
                append_token(&write, TYPE_CONCAT);
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
        append_token(&write, TYPE_CONCAT);
    }

    for (; nalt > 0; --nalt) {
        append_token(&write, TYPE_ALTERNATIVE);
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

void mark_states(struct state *s) {
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

    mark_states(s->o1);
    mark_states(s->o2);
}

void free_state_recursive(struct state *s) {
    if (!s || s == &matchstate) {
        return;
    }

    free_state_recursive(s->o1);
    free_state_recursive(s->o2);
    free(s);
}

void free_state(struct state *s) {
    mark_states(s);

    free_state_recursive(s);
}

struct ptrlist *list1(struct state **s) {
    struct ptrlist *l = malloc(sizeof(*l));
    l->s = s;
    l->next = NULL;
    return l;
}

void patch(struct ptrlist *l, struct state *s) {
    struct ptrlist *next;
    for (struct ptrlist *i = l; i; i = next) {
        next = i->next;
        *i->s = s;
        free(i);
    }
}

struct ptrlist *append(struct ptrlist *l1, struct ptrlist *l2) {
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

void push_frag(struct frag **stackp, struct state *start, struct ptrlist *out) {
    *stackp = frag(start, out, *stackp);
}

struct frag pop_frag(struct frag **stackp) {
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

    struct frag *stackp = NULL,
                e1, e2;
    struct state *s;

    for (; token; token = token->next) {
        switch (token->type) {
        case TYPE_ATOM:
            s = state(STATE_ATOM, NULL, NULL);
            memcpy(s->atom, token->atom, BITNSLOTS(256));
            push_frag(&stackp, s, list1(&s->o1));
            break;
        case TYPE_CONCAT:
            e2 = pop_frag(&stackp);
            e1 = pop_frag(&stackp);
            patch(e1.out, e2.start);
            push_frag(&stackp, e1.start, e2.out);
            break;
        case TYPE_ALTERNATIVE:
            e2 = pop_frag(&stackp);
            e1 = pop_frag(&stackp);
            s = state(STATE_SPLIT, e1.start, e2.start);
            push_frag(&stackp, s, append(e1.out, e2.out));
            break;
        case TYPE_ZERO_MANY:
            e1 = pop_frag(&stackp);
            s = state(STATE_SPLIT, e1.start, NULL);
            patch(e1.out, s);
            push_frag(&stackp, s, list1(&s->o2));
            break;
        case TYPE_ONE_MANY:
            e1 = pop_frag(&stackp);
            s = state(STATE_SPLIT, e1.start, NULL);
            patch(e1.out, s);
            push_frag(&stackp, e1.start, list1(&s->o2));
            break;
        case TYPE_ZERO_ONE:
            e1 = pop_frag(&stackp);
            s = state(STATE_SPLIT, e1.start, NULL);
            push_frag(&stackp, s, append(e1.out, list1(&s->o2)));
            break;
        }
    }

    e1 = pop_frag(&stackp);
    if (stackp) {
        // TODO: free everything
        return NULL;
    }

    patch(e1.out, &matchstate);
    return e1.start;
}

regex_t *regex_compile(char const *pattern) {
    struct regex_token *token = tokenise(pattern);
    struct state *state = token2nfa(token);
    free_token(token);
    regex_t *re = malloc(sizeof(*re));
    re->entry = state;
    return re;
}

void regex_free(regex_t *re) {
    free_state(re->entry);
    free(re);
}

void prepend(struct state_list **l, struct state *s) {
    struct state_list *nl = malloc(sizeof(*nl));
    nl->s = s;
    nl->next = *l;
    *l = nl;
}

void free_list(struct state_list *clist) {
    while (clist) {
        struct state_list *next = clist->next;
        free(clist);
        clist = next;
    }
}

void addstate(struct state_list **l, struct state *s) {
    if (!s){
        return;
    }

    if (s->type == STATE_SPLIT) {
        addstate(l, s->o1);
        addstate(l, s->o2);
        return;
    }

    prepend(l, s);
}

void step(struct state_list *clist, int c, struct state_list **nlist) {
    for (; clist; clist = clist->next) {
        struct state *s = clist->s;
        if (s->type == STATE_ATOM && BITTEST(s->atom, c)) {
            addstate(nlist, s->o1);
        }
    }
}

int regex_match(regex_t *re, char const *s) {
    struct state_list *clist = NULL;
    prepend(&clist, re->entry);

    for (; *s; ++s) {
        struct state_list *nlist = NULL;
        step(clist, *s, &nlist);
        free_list(clist);
        clist = nlist;
    }

    for (; clist; clist = clist->next) {
        if (clist->s->type == STATE_MATCH) {
            free_list(clist);
            return 1;
        }
    }

    free_list(clist);

    return 0;
}

/* vim: set sw=4 et: */
