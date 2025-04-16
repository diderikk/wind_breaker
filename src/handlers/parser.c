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
    session.session->thread_id = pthread_self();

    log_trace("Request %d is being handled by parser", session.session->id);

    if (session.session == NULL) {
      continue;
    }
    assert(session.request != NULL);
    assert(session.response != NULL);
    assert(session.buffer != NULL);

    log_trace("Parsing request:\n%s", session.buffer);

    parse_http_request(session.request, session.buffer);
    session.response->status_code = validate_request_headers(session.request);

    session.session->thread_id = 0;
    push_request(session.session->related_fd, WORK_STATUS_PARSED);
  }

  return NULL;
}
