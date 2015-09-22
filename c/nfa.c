#include <stdlib.h>
#include <string.h>

#include "tokeniser.c"

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

static struct state matchstate = { STATE_MATCH };

static struct ptrlist *ptrlist_alloc(struct state **s) {
    struct ptrlist *l = malloc(sizeof(*l));
    l->s = s;
    l->next = NULL;
    return l;
}

static void ptrlist_patch(struct ptrlist *l, struct state *s) {
    struct ptrlist *next;
    for (struct ptrlist *i = l; i; i = next) {
        next = i->next;
        *i->s = s;
        free(i);
    }
}

static struct ptrlist *ptrlist_concat(struct ptrlist *l1, struct ptrlist *l2) {
    struct ptrlist *oldl1 = l1;

    while (l1->next) {
        l1 = l1->next;
    }
    l1->next = l2;
    return oldl1;
}

static struct frag *frag(struct state *start, struct ptrlist *out, struct frag *prev) {
    struct frag *frag = malloc(sizeof(*frag));
    frag->start = start;
    frag->out = out;
    frag->prev = prev;
    return frag;
}

static void frag_push(struct frag **stackp, struct state *start, struct ptrlist *out) {
    *stackp = frag(start, out, *stackp);
}

static struct frag frag_pop(struct frag **stackp) {
    struct frag frag = **stackp;
    free(*stackp);
    *stackp = frag.prev;
    frag.prev = NULL;
    return frag;
}


static struct state *state(enum state_type type, struct state *o1, struct state *o2) {
    struct state *s = malloc(sizeof(*s));
    s->type = type;
    s->o1 = o1;
    s->o2 = o2;
    return s;
}

static void state_mark_recursive(struct state *s) {
    if (!s || s == &matchstate) {
        return;
    }

    s->type = STATE_MARK;

    if (s->o1 && s->o1->type == STATE_MARK) {
        s->o1 = NULL;
    }

    state_mark_recursive(s->o1);

    if (s->o2 && s->o2->type == STATE_MARK) {
        s->o2 = NULL;
    }

    state_mark_recursive(s->o2);
}

static void state_free_recursive(struct state *s) {
    if (!s || s == &matchstate) {
        return;
    }

    state_free_recursive(s->o1);
    state_free_recursive(s->o2);
    free(s);
}

static void state_free(struct state *s) {
    state_mark_recursive(s);
    state_free_recursive(s);
}

static struct state *token2nfa(struct regex_token *token) {
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

/* vim: set sw=4 et: */
