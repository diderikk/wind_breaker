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

    session.session->thread_id = (long unsigned int)pthread_self();

    log_trace("Request %d (%d) is being handled by request parser",
              session.session->id, session.session->related_fd);

    assert(session.request != NULL);
    assert(session.response != NULL);
    assert(session.buffer != NULL);

    log_trace("Parsing request:\n%s", *session.buffer);

    int result = parse_http_request(session.request, session.buffer);
    if(result == ALLOCATE_MEMORY_ERROR) {
      session.response->status_code = HTTP_INTERNAL_SERVER_ERROR;
    } else if(result < 0) {
      session.response->status_code = HTTP_NOT_ACCEPTABLE;
    } else {
      session.response->status_code = validate_request_headers(session.request);
    }

    log_info("Parsed:\nmethod: %d, uri_tokens: %s %s %s %s, version: %s, "
             "host: %s, user_agent: %s, accept: %s, "
             "accept_language: %s, accept_encoding: %s, connection: %d",
             session.request->method, session.request->uri[0],
             session.request->uri[1], session.request->uri[2],
             session.request->uri[3], session.request->version,
             session.request->host, session.request->user_agent,
             session.request->accept, session.request->accept_language,
             session.request->accept_encoding, session.request->connection);

    session.session->thread_id = 0;
    push_request(session.session->related_fd, WORK_STATUS_PARSED);
  }

  return NULL;
}
