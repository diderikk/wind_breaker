#include "../data_structures/poll_array.h"
#include "../data_structures/session.h"
#include "../shutdown/stop.h"
#include "../socket.h"
#include "../utils/logger.h"
#include "helpers.h"

void *handle_d() {
  struct session_full_return session;
  while (!stop()) {
    session = pop_request(WORK_STATUS_READY_TO_SEND);

    if (session.session == NULL) {
      continue;
    }

    session.session->thread_id = (long unsigned int)pthread_self();

    log_trace("Request %d (%d) is being handled by response sender",
              session.session->id, session.session->related_fd);

    assert(session.request != NULL);
    assert(session.response != NULL);
    assert(session.in_buffer != NULL);
    assert(session.out_buffer != NULL);

    char is_ssl =
        (session.ssl != NULL && SSL_is_init_finished(session.ssl)) ? 1 : 0;

    unsigned int response_size =
        combine_chars(session.out_buffer[0], session.out_buffer[1],
                      session.out_buffer[2], session.out_buffer[3]);

    log_info("Sending response to session %d:\n%s", session.session->id,
             session.out_buffer + 4);

    int send_return =
        (is_ssl) ? send_ssl(session.ssl, session.out_buffer + 4, response_size)
                 : send_bio(session.bio, session.out_buffer + 4, response_size);

    WORK_STATUS next_status = WORK_STATUS_SENT;
    if (is_ssl && send_return <= 0 &&
        SSL_get_error(session.ssl, send_return) == SSL_ERROR_WANT_READ) {
      log_debug("SSL should retry, pushing data back to queue...");
      next_status = WORK_STATUS_SEND_FAILED;
    } else if (!is_ssl && send_return <= 0 &&
               BIO_should_retry(session.bio) == 1) {
      log_debug("BIO should retry, pushing data back to queue...");
      next_status = WORK_STATUS_SEND_FAILED;
    } else if (send_return == -1) {
      log_debug("Could not send data to fd %d, closing connection...",
                session.session->related_fd);
      next_status = WORK_STATUS_REJECTED;
    } else if (session.request->connection == CLOSE) {
      log_debug(
          "Connection close requested, closing connection on fd: %d, id: %d...",
          session.session->related_fd, session.session->id);
      next_status = WORK_STATUS_REJECTED;
    }

    session.session->thread_id = 0;
    if (next_status == WORK_STATUS_SENT) {
      buffer_move_to_front(response_size + 4, session.out_buffer);
    }
    push_request(session.session->related_fd, next_status);
  }
  return NULL;
}
