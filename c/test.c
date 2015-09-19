#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"

     ssize_t getline(char ** restrict linep, size_t * restrict linecapp, FILE * restrict stream);

int main(int argc, char **argv) {
    FILE *f = fopen("tests", "r");
    if (!f) {
        fprintf(stderr, "Could not open tests\n");
        exit(1);
    }

    int passed = 0,
        failed = 0,
        warning = 0;
    regex_t *re = NULL;

    char *re_str;

    ssize_t len;
    char *line = NULL;
    size_t linecap = 0;
    while ((len = getline(&line, &linecap, f)) > 0) {
        if (line[len - 1] == '\n') {
            line[len - 1] = 0;
            --len;
        }

        if (strncmp(line, "regex ", 6) == 0) {
            if (re) {
                regex_free(re);
                free(re_str);
            }

            re_str = strdup(line + 6);
            re = regex_compile(re_str);
        } else if (strncmp(line, "match ", 6) == 0) {
            if (!regex_match(re, line + 6)) {
                fprintf(stderr, "FAIL: /%s/ should match %s\n", re_str, line + 6);
                ++failed;
            } else {
                ++passed;
            }
        } else if (strncmp(line, "differ ", 7) == 0) {
            if (regex_match(re, line + 7)) {
                fprintf(stderr, "FAIL: /%s/ should not match %s\n", re_str, line + 7);
                ++failed;
            } else {
                ++passed;
            }
        } else if (len > 0 && line[0] != '#') {
            fprintf(stderr, "WARN: unknown line: %s\n", line);
            ++warning;
        }
    }

    if (re) {
        regex_free(re);
        free(re_str);
    }

    if (line) {
        free(line);
    }

    printf("%d passed, %d failed", passed, failed);
    if (warning) {
        printf(", %d warning%s", warning, warning == 1 ? "" : "s");
    }
    printf("\n");

    return failed > 0 || warning > 0;
}
