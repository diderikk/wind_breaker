#include "../data_structures/session.h"
#include "../http/response.h"
#include "../properties.h"
#include "../shutdown/stop.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include "../utils/static_file.h"
#include "../version.h"
#include <sqlite3.h>

#define INDEX_PROJECTS_SQL "SELECT id, title FROM Project;"
#define INDEX_POSTS_SQL                                                        \
  "SELECT id, title FROM Post WHERE visibility = 'PUBLIC' ORDER BY "           \
  "created_at DESC;"
#define PROJECT_SQL                                                            \
  "SELECT title,description,githubUrl,imageUrl,websiteUrl,githubReadme FROM "  \
  "Project WHERE id = ?;"
#define POST_SQL "SELECT title, description, created_at FROM Post WHERE id = ?;"
#define VERSION WIND_BREAKER_VERSION

static inline int load_project(sqlite3 *db, sqlite3_stmt **stmt,
                               buffer *tmp_buffer, buffer *buffer,
                               char argument[HTTP_URI_TOKEN_SIZE]);
static inline int load_post(sqlite3 *db, sqlite3_stmt **stmt,
                            buffer *tmp_buffer, buffer *buffer,
                            char argument[HTTP_URI_TOKEN_SIZE]);
static inline int load_index(sqlite3 *db, sqlite3_stmt **index_projects_stmt,
                             sqlite3_stmt **index_posts_stmt,
                             buffer *tmp_buffer, buffer *buffer);
static inline void str_replace(buffer *target, const char *needle,
                               const char *replacement);
static inline unsigned int gen_error_body(http_status_code http_status_code,
                                          buffer *buffer) {
  const char *status_code_str = http_status_code_to_str(http_status_code);
  assert(ENSURE_CAPACITY(buffer, 512) > 0);
  return snprintf(buffer->data, buffer->capacity,
                  "<html><body><h1>%d %s</h1></body></html>", http_status_code,
                  status_code_str);
}

static inline int load(sqlite3 *db, sqlite3_stmt **index_projects_stmt,
                       sqlite3_stmt **index_posts_stmt,
                       sqlite3_stmt **project_stmt, sqlite3_stmt **post_stmt,
                       const char *file_name, buffer *tmp_buffer,
                       buffer *buffer, char argument[HTTP_URI_TOKEN_SIZE]) {
  if (strcmp(file_name, "index.html") == 0) {
    return load_index(db, index_projects_stmt, index_posts_stmt, tmp_buffer,
                      buffer);
  } else if (strcmp(file_name, "project.html") == 0) {
    return load_project(db, project_stmt, tmp_buffer, buffer, argument);
  } else if (strcmp(file_name, "post.html") == 0) {
    return load_post(db, post_stmt, tmp_buffer, buffer, argument);
  }

  return 0;
}

void *handle_b() {

  buffer *tmp_buffer = init_buffer(DEFAULT_BUFFER_SIZE);
  sqlite3 *db = NULL;
  sqlite3_stmt *index_projects_stmt = NULL, *index_posts_stmt = NULL,
               *project_stmt = NULL, *post_stmt = NULL;
  struct session_full_return session;
  assert_log(sqlite3_open_v2(get_db_url(), &db, WB_SQLITE_OPEN_FLAGS, NULL) ==
                 SQLITE_OK,
             "Failed to open database: %s", sqlite3_errmsg(db));

  while (!stop()) {
    session = pop_request(WORK_STATUS_PARSED);

    if (session.session == NULL) {
      continue;
    }

    session.session->thread_id = (long unsigned int)pthread_self();

    log_trace("Request %d (%d) is being handled by response content loader",
              session.session->id, session.session->related_fd);

    assert(session.request != NULL);
    assert(session.response != NULL);
    assert(session.buffer != NULL);

    // Request already parsed
    memset(session.buffer->data, 0, session.buffer->count);

    if (session.response->status_code == HTTP_OK) {
      const char *file_name = uri_to_file_name(session.request->uri);
      read_static_file(file_name, session.buffer);

      if (session.buffer->count == 0) {
        log_error("Failed to read static file: %s", file_name);
        session.response->status_code = HTTP_NOT_FOUND;
        session.buffer->count =
            gen_error_body(session.response->status_code, session.buffer);
      } else {
        // Fetch and inject arguments
        switch (load(db, &index_projects_stmt, &index_posts_stmt, &project_stmt,
                     &post_stmt, file_name, tmp_buffer, session.buffer,
                     session.request->uri[1])) {
        case 0:
          break;
        case -1:
          log_error("Failed to fetch and inject arguments");
          session.response->status_code = HTTP_INTERNAL_SERVER_ERROR;
          session.buffer->count =
              gen_error_body(session.response->status_code, session.buffer);
          break;
        case -2:
          log_error("Id not found in database: %s", session.request->uri[1]);
          session.response->status_code = HTTP_NOT_FOUND;
          session.buffer->count =
              gen_error_body(session.response->status_code, session.buffer);
          break;
        }
      }

    } else {
      session.buffer->count =
          gen_error_body(session.response->status_code, session.buffer);
    }

    session.session->thread_id = 0;
    push_request(session.session->related_fd, WORK_STATUS_DATA_FETCHED);
    memset(tmp_buffer->data, 0, tmp_buffer->count);
  }

  // TODO: Dangling pointers
  sqlite3_finalize(index_projects_stmt);
  sqlite3_finalize(index_posts_stmt);
  sqlite3_finalize(project_stmt);
  // Can be a null pointer
  assert(sqlite3_close_v2(db) == SQLITE_OK);

  return NULL;
}

static inline int load_post(sqlite3 *db, sqlite3_stmt **stmt,
                            buffer *tmp_buffer, buffer *buffer,
                            char argument[HTTP_URI_TOKEN_SIZE]) {

  int rc = 0;
  if (*stmt == NULL) {
    rc = sqlite3_prepare_v2(db, POST_SQL, -1, stmt, NULL);
    if (rc != SQLITE_OK) {
      log_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
      return -1;
    }
  }

  // Maybe malloc
  sqlite3_reset(*stmt);
  sqlite3_clear_bindings(*stmt);
  sqlite3_bind_text(*stmt, 1, argument, -1, SQLITE_STATIC);
  rc = sqlite3_step(*stmt);
  if (rc == SQLITE_ROW) {
    const char *title = (const char *)sqlite3_column_text(*stmt, 0);
    str_replace(buffer, "%TITLE%", title);
    const char *description = (const char *)sqlite3_column_text(*stmt, 1);
    str_replace(buffer, "%DESCRIPTION%", description);
    const char *created_at = (const char *)sqlite3_column_text(*stmt, 2);
    str_replace(buffer, "%CREATED_AT%", created_at);
    char html_file[HTTP_URI_TOKEN_SIZE + 5] = {0};
    snprintf(html_file, HTTP_URI_TOKEN_SIZE + 5, "%s.html", argument);
    read_static_file(html_file, tmp_buffer);
    str_replace(buffer, "%POST%", tmp_buffer->data);
  }

  int reset_rc = sqlite3_reset(*stmt);
  sqlite3_clear_bindings(*stmt);

  if (rc == SQLITE_DONE) {
    return -2;
  } else if (reset_rc != SQLITE_OK) {
    log_error("Failed to reset statement: %s", sqlite3_errmsg(db));
    return -1;
  } else {
    return 0;
  }
}

static inline int load_project(sqlite3 *db, sqlite3_stmt **stmt,
                               buffer *tmp_buffer, buffer *buffer,
                               char argument[HTTP_URI_TOKEN_SIZE]) {

  int rc = 0;
  if (*stmt == NULL) {
    rc = sqlite3_prepare_v2(db, PROJECT_SQL, -1, stmt, NULL);
    if (rc != SQLITE_OK) {
      log_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
      return -1;
    }
  }

  sqlite3_bind_text(*stmt, 1, argument, -1, SQLITE_STATIC);
  rc = sqlite3_step(*stmt);
  if (rc == SQLITE_ROW) {
    const char *title = (const char *)sqlite3_column_text(*stmt, 0);
    str_replace(buffer, "%TITLE%", title);
    const char *description = (const char *)sqlite3_column_text(*stmt, 1);
    str_replace(buffer, "%DESCRIPTION%", description);
    const char *githubUrl = (const char *)sqlite3_column_text(*stmt, 2);
    str_replace(buffer, "%GITHUB_URL%", githubUrl);
    const char *imageUrl = (const char *)sqlite3_column_text(*stmt, 3);
    str_replace(buffer, "%IMAGE_URL%", imageUrl);
    const char *websiteUrl = (const char *)sqlite3_column_text(*stmt, 4);
    if (websiteUrl != NULL) {
      snprintf(tmp_buffer->data, tmp_buffer->capacity,
               "<a href=\"%s\">Website</a>", websiteUrl);
      str_replace(buffer, "%WEBSITE_LINK%", tmp_buffer->data);
      memset(tmp_buffer->data, 0, tmp_buffer->capacity);
    } else {
      str_replace(buffer, "%WEBSITE_LINK%", "");
    }
    const char *githubReadme = (const char *)sqlite3_column_text(*stmt, 5);
    if (githubReadme != NULL) {
      snprintf(tmp_buffer->data, tmp_buffer->capacity,
               "<a href=\"%s\">Readme</a>", githubReadme);
      str_replace(buffer, "%README_LINK%", tmp_buffer->data);
      memset(tmp_buffer->data, 0, tmp_buffer->capacity);
    } else {
      str_replace(buffer, "%README_LINK%", "");
    }
  }

  int reset_rc = sqlite3_reset(*stmt);
  sqlite3_clear_bindings(*stmt);

  if (rc == SQLITE_DONE) {
    return -2;
  } else if (reset_rc != SQLITE_OK) {
    log_error("Failed to reset statement: %s", sqlite3_errmsg(db));
    return -1;
  } else {
    return 0;
  }
}

static inline int load_index(sqlite3 *db, sqlite3_stmt **projects_stmt,
                             sqlite3_stmt **posts_stmt, buffer *tmp_buffer,
                             buffer *buffer) {
  int rc_projects = 0, rc_posts = 0;
  if (*projects_stmt == NULL) {
    rc_projects =
        sqlite3_prepare_v2(db, INDEX_PROJECTS_SQL, -1, projects_stmt, NULL);
    if (rc_projects != SQLITE_OK) {
      log_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
      return -1;
    }
  }
  if (*posts_stmt == NULL) {
    rc_projects = sqlite3_prepare_v2(db, INDEX_POSTS_SQL, -1, posts_stmt, NULL);
    if (rc_projects != SQLITE_OK) {
      log_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
      return -1;
    }
  }

  // Fetch and inject arguments
  memset(tmp_buffer->data, 0, tmp_buffer->capacity);
  tmp_buffer->count = 0;
  while ((rc_projects = sqlite3_step(*projects_stmt)) == SQLITE_ROW) {
    const char *id = (const char *)sqlite3_column_text(*projects_stmt, 0);
    const char *title = (const char *)sqlite3_column_text(*projects_stmt, 1);
    assert(ENSURE_CAPACITY(tmp_buffer, tmp_buffer->count + 256) > 0);
    int offset = snprintf(tmp_buffer->data + tmp_buffer->count,
                          tmp_buffer->capacity - tmp_buffer->count,
                          "<li id=\"%s\"><a href=\"/projects/%s\">%s</a></li>",
                          id, id, title);
    tmp_buffer->count += offset;
  }
  if (rc_projects == SQLITE_DONE) {
    str_replace(buffer, "%PROJECTS%", tmp_buffer->data);
    str_replace(buffer, "%VERSION%", VERSION);

    // Do same for posts
    memset(tmp_buffer->data, 0, tmp_buffer->capacity);
    tmp_buffer->count = 0;
    while ((rc_posts = sqlite3_step(*posts_stmt)) == SQLITE_ROW) {
      const char *id = (const char *)sqlite3_column_text(*posts_stmt, 0);
      const char *title = (const char *)sqlite3_column_text(*posts_stmt, 1);
      assert(ENSURE_CAPACITY(tmp_buffer, tmp_buffer->count + 256) > 0);
      int offset = snprintf(tmp_buffer->data + tmp_buffer->count,
                            tmp_buffer->capacity - tmp_buffer->count,
                            "<li id=\"%s\"><a href=\"/posts/%s\">%s</a></li>",
                            id, id, title);
      tmp_buffer->count += offset;
    }
    if (rc_posts == SQLITE_DONE) {
      str_replace(buffer, "%NOTES%", tmp_buffer->data);
    } else {
      log_error("Failed to fetch posts: %s", sqlite3_errmsg(db));
    }
  } else {
    log_error("Failed to fetch data: %s", sqlite3_errmsg(db));
  }
  rc_projects = sqlite3_reset(*projects_stmt);
  rc_posts = sqlite3_reset(*projects_stmt);

  return (rc_projects != SQLITE_OK || rc_posts != SQLITE_OK) ? -1 : 0;
}

static inline void str_replace(buffer *target, const char *needle,
                               const char *replacement) {
  unsigned int needle_len = strlen(needle);
  unsigned int repl_len = strlen(replacement);
  int diff = repl_len - needle_len;
  unsigned long offset = 0;

  for (unsigned char occurrences = 0; occurrences < 10; occurrences++) {
    char *hit = strstr(target->data + offset, needle);
    unsigned long hit_offset = hit - target->data;

    assert(hit != NULL || occurrences > 0);
    if (hit == NULL) {
      // no more occurrences
      break;
    }
    occurrences++;

    assert(hit >= target->data + offset);
    // Capacity is greater than the difference between needle and replacement
    // word
    log_debug("Hello!, %d", target->count + diff);
    assert(ENSURE_CAPACITY(target, target->count + diff) > 0);

    // If the target->data has been reallocated
    hit = target->data + hit_offset;
    char *after_needle = hit + needle_len;
    // Copy all data after occurence the difference to the right
    memmove(after_needle + diff, after_needle,
            target->count - (hit_offset + needle_len));

    // copy replacement string
    memcpy(hit, replacement, repl_len);
    target->count += diff;

    // adjust pointers, move on
    offset += (hit - target->data) + repl_len;
  }
}
