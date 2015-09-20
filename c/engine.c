#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "tokeniser.h"
#include "nfa.h"

struct regex {
    struct state *entry;
};

struct state_list {
    struct state *s;
    struct state_list *next;
};

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
