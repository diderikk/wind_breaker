#ifndef REGEX_H
#define REGEX_H

#include <regex.h>

int match_regex(const char *pattern, const char *text, int match_count,
                regmatch_t *matches, int flags);

#endif // REGEX_H
