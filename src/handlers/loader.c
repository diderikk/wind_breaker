#include "../data_structures/session.h"
#include "../http/request.h"
#include "../http/response.h"
#include "../shutdown/stop.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include "../utils/static_file.h"

static inline unsigned int gen_error_body(http_status_code http_status_code,
                                          char *buffer) {
  const char *status_code_str = http_status_code_to_str(http_status_code);
  return snprintf(buffer, HTTP_BODY_SIZE,
                  "<html><body><h1>%d %s</h1></body></html>", http_status_code,
                  status_code_str);
}

void *handle_b() {
  struct session_full_return session;
  while (!stop()) {
    session = pop_request(WORK_STATUS_PARSED);
    session.session->thread_id = pthread_self();

    log_trace("Request %d is being handled by content loader",
              session.session->id);

    if (session.session == NULL) {
      continue;
    }
    assert(session.request != NULL);
    assert(session.response != NULL);
    assert(session.buffer != NULL);

    // Request already parsed
    memset(session.buffer, 0, REQUEST_RESPONSE_MAX_SIZE);

    if (session.response->status_code == HTTP_OK) {
      const char *file_name = uri_to_file_name(session.request->uri);
      *session.buffer_size =
          read_static_file(file_name, session.buffer, HTTP_BODY_SIZE);
    } else {
      *session.buffer_size =
          gen_error_body(session.response->status_code, session.buffer);
    }

    session.session->thread_id = 0;
    push_request(session.session->related_fd, WORK_STATUS_DATA_FETCHED);
  }

  return NULL;
}
