#ifndef ENGINE_H
#define ENGINE_H

typedef struct regex regex_t;

regex_t *regex_compile(char const *pattern);
void regex_free(regex_t *re);
int regex_match(regex_t *re, char const *s);

#endif

