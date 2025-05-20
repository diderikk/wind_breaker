#include "../data_structures/session.h"
#include "../http/request.h"
#include "../http/response.h"
#include "../properties.h"
#include "../shutdown/stop.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include "../utils/static_file.h"
#include <sqlite3.h>

#define INDEX_PROJECTS_SQL "SELECT id, title FROM Project;"
#define PROJECT_SQL                                                            \
  "SELECT title,description,githubUrl,imageUrl,websiteUrl,githubReadme FROM "  \
  "Project WHERE id = ?;"

static inline unsigned int str_replace(char *target, const char *needle,
                                       const char *replacement);
static inline unsigned int gen_error_body(http_status_code http_status_code,
                                          char *buffer) {
  const char *status_code_str = http_status_code_to_str(http_status_code);
  return snprintf(buffer, HTTP_BODY_SIZE,
                  "<html><body><h1>%d %s</h1></body></html>", http_status_code,
                  status_code_str);
}

static inline int load_project(sqlite3 *db, sqlite3_stmt **stmt,
                               char *in_out_buffer,
                               unsigned int *in_out_buffer_size,
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
    *in_out_buffer_size = str_replace(in_out_buffer, "%TITLE%", title);
    const char *description = (const char *)sqlite3_column_text(*stmt, 1);
    *in_out_buffer_size =
        str_replace(in_out_buffer, "%DESCRIPTION%", description);
    const char *githubUrl = (const char *)sqlite3_column_text(*stmt, 2);
    if (githubUrl != NULL)
      *in_out_buffer_size =
          str_replace(in_out_buffer, "%GITHUB_URL%", githubUrl);
    const char *imageUrl = (const char *)sqlite3_column_text(*stmt, 3);
    if (imageUrl != NULL)
      *in_out_buffer_size = str_replace(in_out_buffer, "%IMAGE_URL%", imageUrl);
    const char *websiteUrl = (const char *)sqlite3_column_text(*stmt, 4);
    if (websiteUrl != NULL)
      *in_out_buffer_size =
          str_replace(in_out_buffer, "%WEBSITE_URL%", websiteUrl);
    const char *githubReadme = (const char *)sqlite3_column_text(*stmt, 5);
    if (githubReadme != NULL)
      *in_out_buffer_size =
          str_replace(in_out_buffer, "%README_URL%", githubReadme);
  } else if (rc == SQLITE_DONE) {
    log_error("Failed to fetch data: %s", sqlite3_errmsg(db));
    return -2;
  }

  rc = sqlite3_reset(*stmt);
  if (rc != SQLITE_OK) {
    log_error("Failed to fetch data: %s", sqlite3_errmsg(db));
    return -1;
  }
  sqlite3_clear_bindings(*stmt);

  return 0;
}

static inline int load_index(sqlite3 *db, sqlite3_stmt **stmt,
                             char *in_out_buffer,
                             unsigned int *in_out_buffer_size) {
  int rc = 0;
  if (*stmt == NULL) {
    rc = sqlite3_prepare_v2(db, INDEX_PROJECTS_SQL, -1, stmt, NULL);
    if (rc != SQLITE_OK) {
      log_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
      return -1;
    }
  }

  // Fetch and inject arguments
  char tmp_buffer[HTTP_BODY_SIZE];
  unsigned int offset = 0;
  offset += snprintf(tmp_buffer + offset, HTTP_BODY_SIZE - offset, "<ul>");
  while ((rc = sqlite3_step(*stmt)) == SQLITE_ROW) {
    const char *id = (const char *)sqlite3_column_text(*stmt, 0);
    const char *title = (const char *)sqlite3_column_text(*stmt, 1);
    offset += snprintf(tmp_buffer + offset, HTTP_BODY_SIZE - offset,
                       "<li id=\"%s\"><a href=\"/projects/%s\">%s</a></li>", id,
                       id, title);
  }
  if (rc == SQLITE_DONE) {
    offset += snprintf(tmp_buffer + offset, HTTP_BODY_SIZE - offset, "</ul>");
    log_debug("Fetched %s projects", tmp_buffer);

    // sqlite3_finalize(stmt);
    rc = sqlite3_reset(*stmt);
  } else {
    log_error("Failed to fetch data: %s", sqlite3_errmsg(db));
    return -1;
  }

  *in_out_buffer_size = str_replace(in_out_buffer, "%PROJECTS%", tmp_buffer);

  return 0;
}

static inline int load(sqlite3 *db, sqlite3_stmt **index_stmt,
                       sqlite3_stmt **project_stmt, const char *file_name,
                       char *in_out_buffer, unsigned int *in_out_buffer_size,
                       char argument[HTTP_URI_TOKEN_SIZE]) {
  if (strcmp(file_name, "index.html") == 0) {
    return load_index(db, index_stmt, in_out_buffer, in_out_buffer_size);
  } else if (strcmp(file_name, "project.html") == 0) {
    return load_project(db, project_stmt, in_out_buffer, in_out_buffer_size,
                        argument);
  }

  return 0;
}

void *handle_b() {

  sqlite3 *db = NULL;
  sqlite3_stmt *index_stmt = NULL, *project_stmt = NULL;
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
    assert(session.in_buffer != NULL);
    assert(session.out_buffer != NULL);

    // Request already parsed
    memset(session.in_buffer, 0, REQUEST_RESPONSE_MAX_SIZE);

    if (session.response->status_code == HTTP_OK) {
      const char *file_name = uri_to_file_name(session.request->uri);
      *session.in_buffer_size =
          read_static_file(file_name, session.in_buffer, HTTP_BODY_SIZE);

      if (*session.in_buffer_size == 0) {
        log_error("Failed to read static file: %s", file_name);
        session.response->status_code = HTTP_NOT_FOUND;
        *session.in_buffer_size =
            gen_error_body(session.response->status_code, session.in_buffer);
      } else {
        // Fetch and inject arguments
        switch (load(db, &index_stmt, &project_stmt, file_name,
                     session.in_buffer, session.in_buffer_size,
                     session.request->uri[1])) {
        case 0:
          break;
        case -1:
          log_error("Failed to fetch and inject arguments");
          session.response->status_code = HTTP_INTERNAL_SERVER_ERROR;
          *session.in_buffer_size =
              gen_error_body(session.response->status_code, session.in_buffer);
          break;
        case -2:
          log_error("Failed to fetch and inject arguments");
          session.response->status_code = HTTP_NOT_FOUND;
          *session.in_buffer_size =
              gen_error_body(session.response->status_code, session.in_buffer);
          break;
        }
      }

    } else {
      *session.in_buffer_size =
          gen_error_body(session.response->status_code, session.in_buffer);
    }

    session.session->thread_id = 0;
    push_request(session.session->related_fd, WORK_STATUS_DATA_FETCHED);
  }

  return NULL;
}

static inline unsigned int str_replace(char *target, const char *needle,
                                       const char *replacement) {
  char buffer[HTTP_BODY_SIZE] = {0};
  char *insert_point = &buffer[0];
  const char *read_only_target = target;
  unsigned int needle_len = strlen(needle);
  unsigned int repl_len = strlen(replacement);

  for (unsigned char occurrences = 0; occurrences < 10; occurrences++) {
    const char *hit = strstr(read_only_target, needle);

    assert(hit != NULL || occurrences > 0);
    if (hit == NULL) {
      // no more occurrences, copy the rest of the string
      strcpy(insert_point, read_only_target);
      break;
    }
    occurrences++;

    assert(hit >= read_only_target);
    assert(hit < target + HTTP_BODY_SIZE);
    assert(hit + repl_len < target + HTTP_BODY_SIZE);

    // copy part before needle
    memcpy(insert_point, read_only_target, hit - read_only_target);
    insert_point += hit - read_only_target;

    // copy replacement string
    memcpy(insert_point, replacement, repl_len);
    insert_point += repl_len;

    // adjust pointers, move on
    read_only_target = hit + needle_len;
  }

  unsigned int bytes_to_copy = insert_point - buffer;
  // write altered string back to target
  strcpy(target, buffer);

  return bytes_to_copy;
}
