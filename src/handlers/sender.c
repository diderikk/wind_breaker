#include "../data_structures/session.h"
#include "../shutdown/stop.h"
#include "../socket.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"

void *handle_d() {
  struct session_full_return session;
  while (!stop()) {
    session = pop_request(WORK_STATUS_RESPONSE_GENERATED);
    session.session->thread_id = pthread_self();

    log_trace("Request %d is being handled by sender", session.session->id);

    if (session.session == NULL) {
      continue;
    }
    assert(session.request != NULL);
    assert(session.response != NULL);
    assert(session.buffer != NULL);

    char is_ssl =
        (session.ssl != NULL && SSL_is_init_finished(session.ssl)) ? 1 : 0;

    log_trace("Sending response:\n%s", session.buffer);

    int send_return =
        (is_ssl) ? send_ssl(session.ssl, session.buffer, *session.buffer_size)
                 : send_bio(session.bio, session.buffer, *session.buffer_size);

    WORK_STATUS next_status = WORK_STATUS_SENT;
    if (is_ssl && send_return <= 0 &&
        SSL_get_error(session.ssl, send_return) == SSL_ERROR_WANT_READ) {
      log_debug("SSL should retry, pushing data back to queue...");
      next_status = WORK_STATUS_PROCESSING;
    } else if (!is_ssl && send_return <= 0 &&
               BIO_should_retry(session.bio) == 1) {
      log_debug("BIO should retry, pushing data back to queue...");
      next_status = WORK_STATUS_PROCESSING;
    } else if (send_return == -1) {
      log_debug("Could not send data to fd %d, closing connection...",
                session.session->related_fd);
      next_status = WORK_STATUS_REJECTED;
    } else if (session.request->connection == CLOSE) {
      log_debug("Connection close requested, closing connection...");
      next_status = WORK_STATUS_REJECTED;
    }

    session.session->thread_id = 0;
    push_request(session.session->related_fd, next_status);
  }
  return NULL;
}
