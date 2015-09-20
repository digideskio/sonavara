#ifndef NFA_H
#define NFA_H

#include "tokeniser.h"

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

struct state *token2nfa(struct regex_token *token);
void state_free(struct state *s);

#endif
