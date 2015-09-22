#ifndef SONAVARA_ENGINE_INCLUDED
#define SONAVARA_ENGINE_INCLUDED

#include <stdlib.h>
#include <string.h>

#ifndef NO_SELF_CHAIN
#include "nfa.c"
#endif

typedef struct regex {
    struct state *entry;
} regex_t;

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

static void state_list_prepend(struct state_list **l, struct state *s) {
    struct state_list *nl = malloc(sizeof(*nl));
    nl->s = s;
    nl->next = *l;
    *l = nl;
}

static void state_list_free(struct state_list *clist) {
    while (clist) {
        struct state_list *next = clist->next;
        free(clist);
        clist = next;
    }
}

static int state_list_add(struct state_list **l, struct state *s) {
    /* Return true if any added state is STATE_MATCH. */

    if (!s) {
        return 0;
    }

    if (s->type == STATE_SPLIT) {
        int a = state_list_add(l, s->o1);
        int b = state_list_add(l, s->o2);
        return a || b;
    }

    state_list_prepend(l, s);
    return s->type == STATE_MATCH;
}

static int step(struct state_list *clist, int c, struct state_list **nlist) {
    /* Return true if any state added to nlist is STATE_MATCH. */

    int r = 0;

    for (; clist; clist = clist->next) {
        struct state *s = clist->s;
        if (s->type == STATE_ATOM && BITTEST(s->atom, c)) {
            if (state_list_add(nlist, s->o1)) {
                r = 1;
            }
        }
    }

    return r;
}

static int match(regex_t *re, char const *s, int prefix) {
    /* If !prefix, we return 1 or 0 if we match the entire string or not.
     * If prefix, we return the number of characters that generate a match,
     * which may be 0.  If there's no match, return -1. */

    struct state_list *clist = NULL;
    state_list_add(&clist, re->entry);

    int len = 0;
    int longest_match = -1;

    for (; *s; ++s) {
        ++len;

        struct state_list *nlist = NULL;
        int r = step(clist, *s, &nlist);
        state_list_free(clist);
        clist = nlist;

        if (r) {
            longest_match = len;
        }
    }

    /* Exact non-empty match. */
    if (longest_match == len) {
        state_list_free(clist);
        return prefix ? len : 1;
    }

    if (len == 0) {
        // We may match the empty string.
        for (struct state_list *search = clist; search; search = search->next) {
            if (search->s->type == STATE_MATCH) {
                state_list_free(clist);
                return prefix ? 0 : 1;
            }
        }
    }

    state_list_free(clist);

    /* No exact match. */
    return prefix ? longest_match : 0;
}

int regex_match(regex_t *re, char const *s) {
    return match(re, s, 0);
}

int regex_match_prefix(regex_t *re, char const *s) {
    return match(re, s, 1);
}

#endif

/* vim: set sw=4 et: */
