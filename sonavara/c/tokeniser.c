#ifndef SONAVARA_TOKENISER_INCLUDED
#define SONAVARA_TOKENISER_INCLUDED

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

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

struct paren {
    int nalt;
    int natom;
    int opts;
    char const *last;
    struct paren *prev;
};

static void token_append(struct regex_token ***writep, enum regex_token_type type) {
    **writep = malloc(sizeof(***writep));
    (**writep)->type = type;
    (**writep)->next = NULL;
    *writep = &((**writep)->next);
}

static void token_append_atom(struct regex_token ***writep, unsigned char *atom) {
    **writep = malloc(sizeof(***writep));
    (**writep)->type = TYPE_ATOM;
    memcpy((**writep)->atom, atom, BITNSLOTS(256));
    (**writep)->next = NULL;
    *writep = &((**writep)->next);
}

static void token_free(struct regex_token *token) {
    while (token) {
        struct regex_token *next = token->next;
        free(token);
        token = next;
    }
}

static void paren_free(struct paren *paren) {
    while (paren) {
        struct paren *prev = paren->prev;
        free(paren);
        paren = prev;
    }
}

static int process_escape(char const **pattern) {
    int v = 0;

    if (**pattern >= '0' && **pattern <= '7') {
        // Octal.  Why.
        for (int i = 0; i < 3 && **pattern >= '0' && **pattern <= '7'; ++i) {
            v = (v * 8) + (*(*pattern)++ - '0');
        }
        --(*pattern);
    } else if (**pattern == 'x') {
        ++(*pattern);
        for (int i = 0; i < 2 && (isdigit(**pattern) || (tolower(**pattern) >= 'a' && tolower(**pattern) <= 'f')); ++i) {
            int c = tolower(*(*pattern)++);
            if (c >= 'a' && c <= 'f') {
                c = c - 'a' + 10;
            } else {
                c -= '0';
            }
            v = (v * 16) + c;
        }
        --(*pattern);
    } else {
        switch (**pattern) {
        case '0': v = '\0'; break;
        case 'a': v = '\a'; break;
        case 'b': v = '\b'; break;
        case 'f': v = '\f'; break;
        case 'n': v = '\n'; break;
        case 'r': v = '\r'; break;
        case 't': v = '\t'; break;
        case 'v': v = '\v'; break;
        default: v = **pattern; break;
        }
    }

    return v;
}

enum tokeniser_state {
    DEFAULT,
    PAREN_OPTS,
    PAREN_OPTS_DISABLE,
    BRACE_PRE_COMMA,
    BRACE_POST_COMMA,
    BRACE_CCLASS_SUBTRACT,
    BRACE_CCLASS_ADD,
    ESCAPE,
    CCLASS_START,
    CCLASS_MID,
    CCLASS_RANGE,
    CCLASS_ESCAPE,
    CCLASS_POST,
    COMMENT,
    COMMENT_ESCAPE,
};

#define CCLASS_BINARY_SUBTRACT 1
#define CCLASS_BINARY_ADD 2

#define OPT_I (1 << 0)
#define OPT_S (1 << 1)
#define OPT_X (1 << 2)

struct tokeniser {
    enum tokeniser_state state;
    struct regex_token **write;

    struct paren *paren;
    int natom;
    int nalt;
    int opts;
    char const *last;

    char const *brace_start;
    int brace_low;
    int brace_high;

    int cclass_negated;
    int cclass_last;
    int cclass_binary;
    unsigned char cclass_atom[BITNSLOTS(256)];
    unsigned char cclass_atom_parent[BITNSLOTS(256)];
};

static int process(struct tokeniser *sp, char const *pattern, char const *stop);
static int tokenise_default(struct tokeniser *sp, char const **pattern);
static int tokenise_paren_opts(struct tokeniser *sp, int v, int disable);
static int tokenise_escape(struct tokeniser *sp, char const **pattern);
static int tokenise_brace_pre_comma(struct tokeniser *sp, int v);
static int tokenise_brace_post_comma(struct tokeniser *sp, int v);
static int tokenise_cclass_start(struct tokeniser *sp, char const **pattern);
static int tokenise_cclass_mid(struct tokeniser *sp, char const **pattern);
static int tokenise_cclass_range(struct tokeniser *sp, char const **pattern);
static int tokenise_cclass_post(struct tokeniser *sp, char const **pattern);
static void cclass_post_cleanup(struct tokeniser *sp);

static struct regex_token *tokenise(char const *pattern) {
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

static int process(struct tokeniser *sp, char const *pattern, char const *stop) {
    for (; pattern != stop && *pattern; ++pattern) {
        int abort = 0,
            v = *pattern;

        switch (sp->state) {
        case DEFAULT:
            abort = !tokenise_default(sp, &pattern);
            break;

        case PAREN_OPTS:
            abort = !tokenise_paren_opts(sp, v, 0);
            break;

        case PAREN_OPTS_DISABLE:
            abort = !tokenise_paren_opts(sp, v, 1);
            break;

        case BRACE_PRE_COMMA:
            abort = !tokenise_brace_pre_comma(sp, v);
            break;

        case BRACE_POST_COMMA:
            abort = !tokenise_brace_post_comma(sp, v);
            break;

        case BRACE_CCLASS_SUBTRACT:
        case BRACE_CCLASS_ADD:
            if (v != '[') {
                abort = 1;
                break;
            }

            memcpy(sp->cclass_atom_parent, sp->cclass_atom, BITNSLOTS(256));
            int state = sp->state;
            abort = !tokenise_default(sp, &pattern);
            sp->cclass_binary =
                state == BRACE_CCLASS_SUBTRACT ? CCLASS_BINARY_SUBTRACT : CCLASS_BINARY_ADD;
            break;

        case ESCAPE:
            abort = !tokenise_escape(sp, &pattern);
            break;

        case CCLASS_START:
            abort = !tokenise_cclass_start(sp, &pattern);
            break;

        case CCLASS_MID:
            abort = !tokenise_cclass_mid(sp, &pattern);
            break;

        case CCLASS_RANGE:
            abort = !tokenise_cclass_range(sp, &pattern);
            break;

        case CCLASS_ESCAPE:
            sp->state = CCLASS_MID;
            v = process_escape(&pattern);

            if (sp->opts & OPT_I) {
                BITSET(sp->cclass_atom, tolower(v));
                BITSET(sp->cclass_atom, toupper(v));
            } else {
                BITSET(sp->cclass_atom, v);
            }
            break;

        case CCLASS_POST:
            abort = !tokenise_cclass_post(sp, &pattern);
            break;

        case COMMENT:
            if (v == ')') {
                sp->state = DEFAULT;
            } else if (v == '\\') {
                sp->state = COMMENT_ESCAPE;
            }

            break;

        case COMMENT_ESCAPE:
            sp->state = COMMENT;
            break;
        }

        if (abort) {
            return 0;
        }
    }

    return 1;
}

static int tokenise_default(struct tokeniser *sp, char const **pattern) {
    unsigned char atom[BITNSLOTS(256)];

    switch (**pattern) {
    case '{':
        sp->state = BRACE_PRE_COMMA;
        sp->brace_low = -1;
        sp->brace_high = -1;
        sp->brace_start = *pattern;
        break;

    case '\\':
        sp->state = ESCAPE;
        sp->last = *pattern;
        break;

    case '[':
        sp->state = CCLASS_START;
        sp->cclass_negated = 0;
        sp->cclass_last = 0;
        sp->cclass_binary = 0;
        memset(sp->cclass_atom, 0, BITNSLOTS(256));
        sp->last = *pattern;
        break;

    case '(':
        if (strncmp(*pattern, "(?#", 3) == 0) {
            sp->state = COMMENT;
            *pattern += 2;
            break;
        }

        if (strncmp(*pattern, "(?", 2) == 0) {
            ++*pattern;
            sp->state = PAREN_OPTS;
        }

        if (sp->natom > 1) {
            --sp->natom;
            token_append(&sp->write, TYPE_CONCAT);
        }

        struct paren *new_paren = malloc(sizeof(*new_paren));
        new_paren->nalt = sp->nalt;
        new_paren->natom = sp->natom;
        new_paren->opts = sp->opts;
        new_paren->last = *pattern;
        new_paren->prev = sp->paren;
        sp->paren = new_paren;

        sp->nalt = 0;
        sp->natom = 0;
        // sp->opts carries through
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
        sp->opts = sp->paren->opts;
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
        if (!(sp->opts & OPT_S)) {
            BITCLEAR(atom, '\n');
        }
        token_append_atom(&sp->write, atom);
        ++sp->natom;
        sp->last = *pattern;
        break;

    case ' ':
    case '\r':
    case '\n':
    case '\t':
        if (sp->opts & OPT_X) {
            break;
        }

        /* fallthrough */

    default:
        if (sp->natom > 1) {
            --sp->natom;
            token_append(&sp->write, TYPE_CONCAT);
        }
        memset(atom, 0, BITNSLOTS(256));
        if (sp->opts & OPT_I) {
            BITSET(atom, tolower(**pattern));
            BITSET(atom, toupper(**pattern));
        } else {
            BITSET(atom, **pattern);
        }
        token_append_atom(&sp->write, atom);
        ++sp->natom;
        sp->last = *pattern;
        break;
    }

    return 1;
}

static int tokenise_paren_opts(struct tokeniser *sp, int v, int disable) {
    int opt = 0;

    if (v == ':') {
        sp->state = DEFAULT;
        return 1;
    } else if (v == '-' && !disable) {
        sp->state = PAREN_OPTS_DISABLE;
        return 1;
    }
    
    if (v == 'i') {
        opt = OPT_I;
    } else if (v == 's') {
        opt = OPT_S;
    } else if (v == 'x') {
        opt = OPT_X;
    }

    if (!opt) {
        return 0;
    }

    if (!disable) {
        sp->opts |= opt;
    } else {
        sp->opts &= ~opt;
    }

    return 1;
}

static int tokenise_escape(struct tokeniser *sp, char const **pattern) {
    sp->state = DEFAULT;

    if (sp->natom > 1) {
        --sp->natom;
        token_append(&sp->write, TYPE_CONCAT);
    }

    unsigned char atom[256];

    int v = process_escape(pattern);

    memset(atom, 0, BITNSLOTS(256));
    if (sp->opts & OPT_I) {
        BITSET(atom, toupper(v));
        BITSET(atom, tolower(v));
    } else {
        BITSET(atom, v);
    }

    token_append_atom(&sp->write, atom);
    ++sp->natom;

    return 1;
}

static int tokenise_brace_pre_comma(struct tokeniser *sp, int v) {
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

static int tokenise_brace_post_comma(struct tokeniser *sp, int v) {
    if (v == '}') {
        sp->state = DEFAULT;

        char const *last = sp->last,
             *brace_start = sp->brace_start;

        int brace_low = sp->brace_low < 1 ? 0 : sp->brace_low,
            brace_high = sp->brace_high;

        // Note that one instance of the to-be-repeated content is already on
        // the token stream.

        if (brace_low == 0 && brace_high == -1) {
            if (!process(sp, "*", NULL)) {
                return 0;
            }
        } else if (brace_low == 1 && brace_high == -1) {
            if (!process(sp, "+", NULL)) {
                return 0;
            }
        } else if (brace_high == -1) {
            for (int i = 1; i < brace_low; ++i) {
                if (!process(sp, last, brace_start)) {
                    return 0;
                }
            }
            if (!process(sp, "+", NULL)) {
                return 0;
            }
        } else {
            if (brace_low == 0) {
                if (!process(sp, "?", NULL)) {
                    return 0;
                }
                --brace_high;
            }

            for (int i = 1; i < brace_low; ++i) {
                if (!process(sp, last, brace_start)) {
                    return 0;
                }
            }

            for (int i = brace_low; i < brace_high; ++i) {
                if (!process(sp, last, brace_start)) {
                    return 0;
                }
                if (!process(sp, "?", NULL)) {
                    return 0;
                }
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

static int tokenise_cclass_start(struct tokeniser *sp, char const **pattern) {
    sp->state = CCLASS_MID;

    if (**pattern == '^') {
        sp->cclass_negated = 1;
        return 1;
    }

    return tokenise_cclass_mid(sp, pattern);
}

static struct cclass_fn {
    char const *match;
    int (*fn)(int);
} cclass_fns[] = {
    {"alnum", isalnum},
    {"alpha", isalpha},
    {"blank", isblank},
    {"cntrl", iscntrl},
    {"digit", isdigit},
    {"graph", isgraph},
    {"lower", islower},
    {"print", isprint},
    {"punct", ispunct},
    {"space", isspace},
    {"upper", isupper},
    {"xdigit", isxdigit},
    {NULL, NULL},
};

static int attempt_cclass_expr(struct tokeniser *sp, char const **pattern) {
    *pattern += 2;

    int negate = **pattern == '^';
    if (negate) {
        ++*pattern;
    }

    for (struct cclass_fn *fn = cclass_fns; fn->match; ++fn) {
        int matchlen = strlen(fn->match);
        if (strncmp(*pattern, fn->match, matchlen) == 0) {
            if (strncmp(*pattern + matchlen, ":]", 2) == 0) {
                *pattern += matchlen + 1;

                if (sp->opts & OPT_I && negate && (fn->fn == islower || fn->fn == isupper)) {
                    return 0;
                }

                for (int i = 0; i < 256; ++i) {
                    if (negate) {
                        if (!fn->fn(i)) {
                            BITSET(sp->cclass_atom, i);
                        }
                    } else {
                        if (fn->fn(i)) {
                            if (sp->opts & OPT_I) {
                                BITSET(sp->cclass_atom, tolower(i));
                                BITSET(sp->cclass_atom, toupper(i));
                            } else {
                                BITSET(sp->cclass_atom, i);
                            }
                        }
                    }
                }

                return 1;
            }
        }
    }

    return 0;
}

static int tokenise_cclass_mid(struct tokeniser *sp, char const **pattern) {
    switch (**pattern) {
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

        if (sp->cclass_binary) {
            if (sp->cclass_binary == CCLASS_BINARY_SUBTRACT) {
                for (int i = 0; i < 256; ++i) {
                    if (BITTEST(sp->cclass_atom, i)) {
                        BITCLEAR(sp->cclass_atom_parent, i);
                    }
                }
            } else if (sp->cclass_binary == CCLASS_BINARY_ADD) {
                for (int i = 0; i < 256; ++i) {
                    if (BITTEST(sp->cclass_atom, i)) {
                        BITSET(sp->cclass_atom_parent, i);
                    }
                }
            }

            memcpy(sp->cclass_atom, sp->cclass_atom_parent, BITNSLOTS(256));
        }
        break;

    case '-':
        if (!sp->cclass_last) {
            sp->cclass_last = **pattern;
            BITSET(sp->cclass_atom, sp->cclass_last);
        } else {
            sp->state = CCLASS_RANGE;
        }
        break;

    default:
        if (strncmp(*pattern, "[:", 2) == 0) {
            return attempt_cclass_expr(sp, pattern);
        }

        sp->cclass_last = **pattern;
        if (sp->opts & OPT_I) {
            BITSET(sp->cclass_atom, tolower(**pattern));
            BITSET(sp->cclass_atom, toupper(**pattern));
        } else {
            BITSET(sp->cclass_atom, **pattern);
        }
        break;
    }

    return 1;
}

static int tokenise_cclass_range(struct tokeniser *sp, char const **pattern) {
    if (**pattern == ']') {
        BITSET(sp->cclass_atom, **pattern);
        return tokenise_cclass_mid(sp, pattern);
    }

    for (int i = sp->cclass_last; i <= **pattern; ++i) {
        if (sp->opts & OPT_I) {
            BITSET(sp->cclass_atom, tolower(i));
            BITSET(sp->cclass_atom, toupper(i));
        } else {
            BITSET(sp->cclass_atom, i);
        }
    }

    sp->cclass_last = 0;
    sp->state = CCLASS_MID;
    return 1;
}

static int tokenise_cclass_post(struct tokeniser *sp, char const **pattern) {
    if (strncmp(*pattern, "{-}", 3) == 0) {
        *pattern += 2;
        sp->state = BRACE_CCLASS_SUBTRACT;
        return 1;
    }

    if (strncmp(*pattern, "{+}", 3) == 0) {
        *pattern += 2;
        sp->state = BRACE_CCLASS_ADD;
        return 1;
    }

    cclass_post_cleanup(sp);

    return tokenise_default(sp, pattern);
}

static void cclass_post_cleanup(struct tokeniser *sp) {
    if (sp->natom > 1) {
        --sp->natom;
        token_append(&sp->write, TYPE_CONCAT);
    }

    token_append_atom(&sp->write, sp->cclass_atom);
    ++sp->natom;
    sp->state = DEFAULT;
}

#endif

/* vim: set sw=4 et: */
