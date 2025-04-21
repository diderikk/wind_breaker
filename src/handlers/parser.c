#include "../data_structures/session.h"
#include "../http/request.h"
#include "../shutdown/stop.h"
#include "../static.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"

void *handle_a() {
  struct session_full_return session;
  while (!stop()) {
    session = pop_request(WORK_STATUS_REQUEST_READ);

    if (session.session == NULL) {
      continue;
    }

    session.session->thread_id = pthread_self();

    log_trace("Request %d (%d) is being handled by request parser",
              session.session->id, session.session->related_fd);

    assert(session.request != NULL);
    assert(session.response != NULL);
    assert(session.in_buffer != NULL);

    log_trace("Parsing request:\n%s", session.in_buffer);

    parse_http_request(session.request, session.in_buffer);
    session.response->status_code = validate_request_headers(session.request);

    log_info("Parsed:\nmethod: %d, uri: %s, version: %s, "
             "host: %s, user_agent: %s, accept: %s, "
             "accept_language: %s, accept_encoding: %s, connection: %d",
             session.request->method, session.request->uri,
             session.request->version, session.request->host,
             session.request->user_agent, session.request->accept,
             session.request->accept_language, session.request->accept_encoding,
             session.request->connection);

    session.session->thread_id = 0;
    push_request(session.session->related_fd, WORK_STATUS_PARSED);
  }

  return NULL;
}
