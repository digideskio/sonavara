#ifndef TOKENISER_H
#define TOKENISER_H

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

void token_free(struct regex_token *token);
struct regex_token *tokenise(char const *pattern);

#endif
