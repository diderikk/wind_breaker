#include "../data_structures/session.h"
#include "../http/response.h"
#include "../shutdown/stop.h"
#include "../utils/logger.h"
#include "helpers.h"

void *handle_c() {
  struct session_full_return session;
  while (!stop()) {
    session = pop_request(WORK_STATUS_DATA_FETCHED);

    if (session.session == NULL) {
      continue;
    }

    session.session->thread_id = pthread_self();

    log_trace("Request %d (%d) is being handled by response builder",
              session.session->id, session.session->related_fd);

    assert(session.request != NULL);
    assert(session.response != NULL);
    assert(session.in_buffer != NULL);
    assert(session.out_buffer != NULL);

    char is_ssl =
        (session.ssl != NULL && SSL_is_init_finished(session.ssl)) ? 1 : 0;
    char should_be_ssl = (session.ssl != NULL && !is_ssl) ? 1 : 0;

    if (should_be_ssl) {
      *session.in_buffer_size = construct_upgrade_to_https_response(
          session.request->uri, session.request->host, session.in_buffer);
    } else {
      *session.in_buffer_size = construct_response(
          session.response, session.request->uri,
          session.request->accept_encoding, session.request->if_none_match,
          session.in_buffer, *session.in_buffer_size);
    }

    WORK_STATUS next_status = WORK_STATUS_READY_TO_SEND;
    unsigned int out_buffer_offset = seek_buffer_offset(session.out_buffer);
    if (out_buffer_offset + *session.in_buffer_size + 4 >
        REQUEST_RESPONSE_MAX_SIZE) {
      log_info("No more space in out buffer...");
      next_status = WORK_STATUS_PROCESSING;
    } else {
      out_buffer_offset = append_response_length(
          out_buffer_offset, session.out_buffer, *session.in_buffer_size);
      memcpy(session.out_buffer + out_buffer_offset, session.in_buffer,
             *session.in_buffer_size);
    }

    session.session->thread_id = 0;
    push_request(session.session->related_fd, next_status);
  }

  return NULL;
}
