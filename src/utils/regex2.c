#include "regex2.h"
#include "logger.h"

int match_regex(const char *pattern, char *text, int match_count,
                regmatch_t *matches, int flags) {
  regex_t regex;
  int ret = regcomp(&regex, pattern, REG_EXTENDED);
  if (ret) {
    log_error("Could not compile regex");
    return -1;
  }

  // Execute the regular expression
  ret = regexec(&regex, text, match_count, matches, flags);
  if (!ret) {
    // printf("Match found:\n");

    // Print the entire match
    //        for (int i = 0; i < match_count; i++) {
    //            if (matches[i].rm_so != -1) {
    //                printf("Match %d: %.*s\n", i, matches[i].rm_eo -
    //                matches[i].rm_so, text + matches[i].rm_so);
    //            }
    //        }
  } else if (ret == REG_NOMATCH) {
    log_warn("No match for regex: %s on %s", pattern, text);
    return -1;
  } else {
    char errbuf[100];
    regerror(ret, &regex, errbuf, sizeof(errbuf));
    log_error("Regex match failed: %s", errbuf);
    return -1;
  }

  // Free the compiled regular expression
  regfree(&regex);

  return match_count;
}
