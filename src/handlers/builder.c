#include "../data_structures/session.h"
#include "../http/response.h"
#include "../shutdown/stop.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"

void *handle_c() {
  struct session_full_return session;
  while (!stop()) {
    session = pop_request(WORK_STATUS_DATA_FETCHED);
    session.session->thread_id = pthread_self();

    log_trace("Request %d is being handled by content loader",
              session.session->id);

    if (session.session == NULL) {
      continue;
    }
    assert(session.request != NULL);
    assert(session.response != NULL);
    assert(session.buffer != NULL);

    char is_ssl =
        (session.ssl != NULL && SSL_is_init_finished(session.ssl)) ? 1 : 0;
    char should_be_ssl = (session.ssl != NULL && !is_ssl) ? 1 : 0;

    if (should_be_ssl) {
      *session.buffer_size = construct_upgrade_to_https_response(
          session.request->uri, session.request->host, session.buffer);
    } else {
      *session.buffer_size = construct_response(
          session.response, session.request->uri,
          session.request->accept_encoding, session.request->if_none_match,
          session.buffer, *session.buffer_size);
    }

    session.session->thread_id = 0;
    push_request(session.session->related_fd, WORK_STATUS_RESPONSE_GENERATED);
  }

  return NULL;
}
