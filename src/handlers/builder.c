#include "../data_structures/session.h"
#include "../http/response.h"
#include "../shutdown/stop.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"

void *handle_c() {
  struct session_full_return session;
  buffer *tmp_buffer = init_buffer(0);
  while (!stop()) {
    memset(tmp_buffer->data, 0, tmp_buffer->capacity);
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

    // Deprecated
    if (session.response->status_code == HTTP_MOVED_PERMANENTLY) {
      construct_upgrade_to_https_response(
          session.request->uri, session.request->host, session.buffer);
    } else {
      construct_response(session.response, session.request->uri,
                         session.request->accept_encoding,
                         session.request->if_none_match, session.buffer,
                         tmp_buffer);
    }

    WORK_STATUS next_status = WORK_STATUS_READY_TO_SEND;
    session.session->thread_id = 0;
    push_request(session.session->related_fd, next_status);
  }

  deinit_buffer(tmp_buffer);
  tmp_buffer = NULL;

  return NULL;
}
