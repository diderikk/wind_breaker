#include "../data_structures/session.h"
#include "../http/response.h"
#include "../shutdown/stop.h"
#include "../utils/logger.h"
#include "helpers.h"

void *handle_c() {
  struct session_full_return session;
  char *tmp_buffer = malloc(REQUEST_RESPONSE_MAX_SIZE);
  while (!stop()) {
    memset(tmp_buffer, 0, REQUEST_RESPONSE_MAX_SIZE);
    session = pop_request(WORK_STATUS_DATA_FETCHED);

    if (session.session == NULL) {
      continue;
    }

    session.session->thread_id = (long unsigned int)pthread_self();

    log_trace("Request %d (%d) is being handled by response builder",
              session.session->id, session.session->related_fd);

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
          session.buffer, *session.buffer_size, tmp_buffer);
    }

    WORK_STATUS next_status = WORK_STATUS_READY_TO_SEND;
    session.session->thread_id = 0;
    push_request(session.session->related_fd, next_status);
  }

  free(tmp_buffer);

  return NULL;
}
