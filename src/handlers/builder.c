#include "../data_structures/session.h"
#include "../data_structures/session_by_thread_map.h"
#include "../http/response.h"
#include "../shutdown/stop.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"

void *handle_c() {
  session_t *session;
  buffer *tmp_buffer = init_buffer(0);
  while (!stop()) {
    memset(tmp_buffer->data, 0, tmp_buffer->capacity);
    session = pop_request(WORK_STATUS_DATA_FETCHED);

    if (session == NULL) {
      continue;
    }

    put_session_id_by_thread(session->meta.id);

    log_trace("Request %d (%d) is being handled by response builder",
              session->meta.id, session->meta.related_fd);

    assert(session->buffer != NULL);

    // Deprecated
    if (session->response.status_code == HTTP_MOVED_PERMANENTLY) {
      construct_upgrade_to_https_response(
          session->request.uri, session->request.host, session->buffer);
    } else {
      construct_response(&session->response, session->request.uri,
                         session->request.accept_encoding,
                         session->request.if_none_match, session->buffer,
                         tmp_buffer);
    }

    WORK_STATUS next_status = WORK_STATUS_READY_TO_SEND;
    put_session_id_by_thread(-1);
    push_request(session->meta.related_fd, next_status);
  }

  deinit_buffer(tmp_buffer);
  tmp_buffer = NULL;

  return NULL;
}
