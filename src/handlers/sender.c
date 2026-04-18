#include "../data_structures/session.h"
#include "../shutdown/stop.h"
#include "../socket.h"
#include "../utils/assert2.h"
#include "../utils/logger.h"
#include <string.h>

void *handle_d() {
  session_t *session;
  while (!stop()) {
    session = pop_request(WORK_STATUS_READY_TO_SEND);

    if (session == NULL) {
      continue;
    }

    session->meta.thread_id = (long unsigned int)pthread_self();

    log_trace("Request %d (%d) is being handled by response sender",
              session->meta.id, session->meta.related_fd);

    assert(session->buffer != NULL);

    char is_ssl = session->request.is_ssl;
    log_info("Sending response to session %d:\n%.*s", session->meta.id,
             session->buffer->count, session->buffer->data);

    int send_return = (is_ssl) ? send_ssl(session->ssl, session->buffer)
                               : send_bio(session->bio, session->buffer);

    WORK_STATUS next_status = WORK_STATUS_SENT;
    log_debug("send_return: %d, buffer_size: %lu", send_return,
              session->buffer->count);
    if (is_ssl && send_return <= 0) {
      int ssl_error = SSL_get_error(session->ssl, send_return);
      if (ssl_error == SSL_ERROR_WANT_READ) {
        log_debug("SSL should retry read, pushing data back to queue...");
        next_status = WORK_STATUS_SEND_FAILED;
      } else if (ssl_error == SSL_ERROR_WANT_WRITE) {
        log_debug("SSL should retry write, pushing data back to queue...");
        next_status = WORK_STATUS_READY_TO_SEND;
      } else {
        log_debug("Could not send data to fd %d, closing connection...",
                  session->meta.related_fd);
        next_status = WORK_STATUS_REJECTED;
      }
    } else if (!is_ssl && send_return <= 0 &&
               BIO_should_retry(session->bio) == 1) {
      log_debug("BIO should retry, pushing data back to queue...");
      next_status = WORK_STATUS_SEND_FAILED;
    } else if (send_return < session->buffer->count) {
      log_debug("Partial send, pushing data back to sending queue...");
      memmove(session->buffer->data, session->buffer->data + send_return,
              session->buffer->count - send_return);
      session->buffer->count -= send_return;
      next_status = WORK_STATUS_READY_TO_SEND;
    } else if (send_return == -1) {
      log_debug("Could not send data to fd %d, closing connection...",
                session->meta.related_fd);
      next_status = WORK_STATUS_REJECTED;
    } else if (session->request.connection == CLOSE) {
      log_debug(
          "Connection close requested, closing connection on fd: %d, id: %d...",
          session->meta.related_fd, session->meta.id);
      next_status = WORK_STATUS_REJECTED;
    }

    session->meta.thread_id = 0;
    push_request(session->meta.related_fd, next_status);
  }
  return NULL;
}
