#define _GNU_SOURCE
#include "listener.h"
#include "data_structures/poll_array.h"
#include "data_structures/session.h"
#include "data_structures/worker_queue.h"
#include "http/request.h"
#include "http/response.h"
#include "shutdown/stop.h"
#include "socket.h"
#include "utils/assert2.h"
#include "utils/logger.h"
#include "worker.h"
#include <errno.h>

#define INT_MOST_SIGNIFICANT_BIT 1 << 31
#define INT_SECOND_MOST_SIGNIFICANT_BIT 1 << 30

void *worker_function(void *_arg);
POLL_ERROR_CLASS classify_poll_error(int code);
const char *get_poll_event_description(short event);
int check_for_socket_error(int fd);

int new_connection_handler(SSL *ssl, BIO *bio) { return 0; }
void close_connection_handler(int fd) {}
int handle_request_async(int fd, char *buffer) {
  struct session_full_return session = get_session_sync(fd);
  assert(session.session != NULL);
  assert(session.bio != NULL);
  int recv_return = recv_bio(session.bio, buffer, REQUEST_RESPONSE_MAX_SIZE);
  if (recv_return > 0) {
    queue_push(fd, buffer, recv_return);
    return 0;
  } else if (recv_return == -1 && BIO_should_retry(session.bio) == 1) {
    return 0;
  } else {
    // Got error or connection closed by client
    if (recv_return == 0) {
      // Connection closed
      log_trace("Socket %d hung up", fd);
    }
    return -1;
  }
}

void listen_async(int listener) {
  _listen(listener, new_connection_handler, close_connection_handler,
          handle_request_async);
}

int get_listener_socket(const char *port, int backlog) {
  assert(port != NULL);

  struct addrinfo hints, *servinfo, *p;
  int socket_fd, return_val;
  int yes = 1;
  char ip_str[INET6_ADDRSTRLEN];
  // https://beej.us/guide/bgnet/html/split/client-server-background.html
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;       // IPv4, AF_INET6: IPv6 and AF_UNSPECT: BOTH
  hints.ai_socktype = SOCK_STREAM; // TCP
  hints.ai_flags = AI_PASSIVE;     // Use for binding to any local address.

  // https://man7.org/linux/man-pages/man3/getaddrinfo.3.html
  // Simply put: fetches all possible hosts that the socket can be bound to
  // based on the options selected above
  assert_log(return_val = getaddrinfo(NULL, port, &hints, &servinfo) == 0,
             "getaddrinfo: %s\n", gai_strerror(return_val));

  // Iterates through all possible hosts and tries to bind a socket
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((socket_fd = open_socket(p)) < 0)
      continue;

    // allows the socket to bind to an address that is in a TIME_WAIT state.
    // Allows for quick server restarts
    assert(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) !=
           -1);

    if ((bind_socket(socket_fd, p)) < 0)
      continue;

    break;
  }
  assert(p != NULL);

  // Begins listening, BACKLOG is the max amount of waiting connections
  listen_socket(socket_fd, backlog);

  get_in_addr_str(p->ai_addr, ip_str, sizeof(ip_str));
  log_info("Listening for connections on: Address: %s, Port %d", ip_str,
           get_in_addr_port(p->ai_addr));

  freeaddrinfo(servinfo);
  servinfo = NULL;
  p = NULL;

  return socket_fd;
}

void _listen(int listener, int (*new_connection_handler)(SSL *, BIO *),
             void (*close_connection_handler)(int),
             int (*request_handler)(int, char *)) {
  assert(listener > 0);
  assert(request_handler != NULL);

  char ip_str[INET6_ADDRSTRLEN], data[REQUEST_RESPONSE_MAX_SIZE];
  char event_count;
  int client_socket_fd, new_conn_ret;
  sigset_t sigmask;
  struct sockaddr_storage client_addr;
  struct pollfd *poll;
  poll_array *poll_fd_array = NULL;
  POLL_ERROR_CLASS error_class;

  event_count = 0;
  memset(data, 0, REQUEST_RESPONSE_MAX_SIZE);
  memset(ip_str, 0, INET6_ADDRSTRLEN);
  sigemptyset(&sigmask);
  init_poll_array(&poll_fd_array, listener);
  assert(poll_fd_array != NULL);

  // Continously listen for new connections
  while (!stop()) {
    assert(poll_fd_array->count > 0);
    log_info("Number of active sockets (including listener): %d",
             poll_fd_array->count);

    // https://man7.org/linux/man-pages/man2/poll.2.html
    // NULL causes the poll system call to poll until a revents
    // is updated by the kernel
    event_count =
        ppoll(poll_fd_array->fds, poll_fd_array->count, NULL, &sigmask);

    if (event_count < 0) {
      error_class = classify_poll_error(errno);
      assert(error_class != RESET);

      continue;
    }

    // Iterate through all the file descriptors and check for events
    for (int i = 0; i < poll_fd_array->count; i++) {
      poll = &poll_fd_array->fds[i];
      assert(poll != NULL);

      if (check_for_socket_error(poll->fd) == -1) {
        log_debug("Socket %d is invalid, removing...", poll->fd);
        close_connection_handler(poll->fd);
        del_from_session_sync(poll->fd);
        remove_poll_fd_by_index_sync(poll_fd_array, &i);
        continue;
      }

      // ERROR HANDLING
      if (poll->revents & POLLERR) {
        error_class = classify_poll_error(errno);
        assert(error_class != RESET);
        if (error_class == REMOVE_FD) {
          log_debug("Socket %d error event, removing...", poll->fd);
          close_connection_handler(poll->fd);
          del_from_session_sync(poll->fd);
          remove_poll_fd_by_index_sync(poll_fd_array, &i);
          continue;
        }
      }
      if (poll->revents & (POLLNVAL | POLLHUP | POLLRDHUP)) {
        if (poll->revents & POLLHUP)
          log_debug("%s", get_poll_event_description(POLLHUP));
        else if (poll->revents & POLLRDHUP)
          log_debug("%s", get_poll_event_description(POLLRDHUP));
        else if (poll->revents & POLLNVAL)
          log_debug("%s", get_poll_event_description(POLLNVAL));
        if (check_for_socket_error(poll->fd) == -1) {
          close_connection_handler(poll->fd);
          del_from_session_sync(poll->fd);
          remove_poll_fd_by_index_sync(poll_fd_array, &i);
          continue;
        }
      }
      if (poll->revents & POLLOUT) {
        log_debug("Socket %d is ready for writing", poll->fd);
        // Should already be data in worker queue, set it to be ready
        set_work_ready(poll->fd | INT_SECOND_MOST_SIGNIFICANT_BIT |
                       INT_MOST_SIGNIFICANT_BIT);
        continue;
      }
      // HANDLES NEW SOCKET EVENTS
      if (poll->revents & POLLIN) {
        // New connection wants to connect from the accept.
        if (poll->fd == listener) {

          log_debug("Polling for new client to connect...");
          client_socket_fd =
              accept_socket(listener, (struct sockaddr *)&client_addr);

          if (client_socket_fd < 0)
            continue;

          get_in_addr_str((struct sockaddr *)&client_addr, ip_str,
                          sizeof(ip_str));
          log_info("Client connect %s:%d", ip_str,
                   get_in_addr_port((struct sockaddr *)&client_addr));
          struct session_full_return full_session =
              add_to_session_sync(client_socket_fd);
          int poll_array_index =
              add_poll_fd_sync(poll_fd_array, client_socket_fd);
          new_conn_ret =
              new_connection_handler(full_session.ssl, full_session.bio);
          if (new_conn_ret == -1) {
            log_trace("New connection handler failed for fd %d",
                      client_socket_fd);
            del_from_session_sync(client_socket_fd);
            remove_poll_fd_by_index_sync(poll_fd_array, &poll_array_index);
          }
          continue;
        } else {
          log_debug("Polling for existing fd %d to send data...", poll->fd);

          if (request_handler(poll->fd, data) != -1) {
            memset(data, 0, REQUEST_RESPONSE_MAX_SIZE);
            continue;
          }

          close_connection_handler(poll->fd);
          del_from_session_sync(poll->fd);
          remove_poll_fd_by_index_sync(poll_fd_array, &i);
          continue;
        }
      }
    }
  }
  destroy_poll_array(&poll_fd_array);
}

void *listener_worker_function(void *_arg) {
  // worker_arg *arg = (worker_arg *)_arg;
  int send_return, response_code, response_size, original_fd;
  struct session_full_return session;
  worker_data *data;
  http_request_t *http_request = malloc(sizeof(http_request_t));
  char *tmp_response_buffer = malloc(REQUEST_RESPONSE_MAX_SIZE);

  assert(tmp_response_buffer != NULL);
  assert(http_request != NULL);

  memset(tmp_response_buffer, 0, REQUEST_RESPONSE_MAX_SIZE);
  memset(http_request, 0, sizeof(http_request_t));
  response_code = 500;
  original_fd = 0;

  while (!stop()) {
    data = queue_pop();
    if (data == NULL) {
      continue;
    }
    if ((data->fd & INT_MOST_SIGNIFICANT_BIT) == 0)
      original_fd = data->fd;
    else
      original_fd = data->fd & ~(INT_MOST_SIGNIFICANT_BIT);

    session = add_thread_to_session(original_fd);
    assert(session.bio != NULL);
    log_info("Handled by worker: %lu", (unsigned long)pthread_self());
    char is_ssl =
        (session.ssl != NULL && SSL_is_init_finished(session.ssl)) ? 1 : 0;
    char should_be_ssl = (session.ssl != NULL && !is_ssl) ? 1 : 0;

    if ((data->fd & INT_MOST_SIGNIFICANT_BIT) == 0) {
      parse_http_request(http_request, data->data);
      response_code = validate_request_headers(http_request);

      memset(data->data, 0, REQUEST_RESPONSE_MAX_SIZE);
      if (should_be_ssl) {
        response_size = construct_upgrade_to_https_response(
            http_request->uri, http_request->host, data->data);
      } else {
        response_size = construct_response(
            response_code, http_request->uri, http_request->accept_encoding,
            http_request->if_none_match, data->data, tmp_response_buffer);
      }
    } else {
      response_size = data->size;
    }

    send_return = (is_ssl) ? send_ssl(session.ssl, data->data, response_size)
                           : send_bio(session.bio, data->data, response_size);

    if (is_ssl && send_return <= 0 &&
        SSL_get_error(session.ssl, send_return) == SSL_ERROR_WANT_READ) {
      log_debug("SSL should retry, pushing data back to queue...");
      // Most significant = write work, Second most significant = not ready
      queue_push(original_fd | INT_MOST_SIGNIFICANT_BIT |
                     INT_SECOND_MOST_SIGNIFICANT_BIT,
                 data->data, response_size);

    } else if (!is_ssl && send_return <= 0 &&
               BIO_should_retry(session.bio) == 1) {
      log_debug("BIO should retry, pushing data back to queue...");
      // Most significant = write work, Second most significant = not ready
      queue_push(original_fd | INT_MOST_SIGNIFICANT_BIT |
                     INT_SECOND_MOST_SIGNIFICANT_BIT,
                 data->data, response_size);
    } else if (send_return == -1) {
      log_debug("Could not send data to fd %d, closing connection...",
                original_fd);
      close(original_fd);
    } else if (http_request->connection == CLOSE) {
      log_debug("Connection close requested, closing connection...");
      close(original_fd);
    } else if (should_be_ssl) {
      log_debug("Upgrading connection to SSL...");
      // close(original_fd);
    }

    remove_thread_from_session();

    // Reset worker queue data
    memset(data->data, 0, REQUEST_RESPONSE_MAX_SIZE);
    data->fd = 0;
    data->size = 0;

    // Reset buffers
    memset(tmp_response_buffer, 0, REQUEST_RESPONSE_MAX_SIZE);
    memset(http_request, 0, sizeof(http_request_t));
    response_code = 500;
    response_size = 0;
    original_fd = 0;
  }

  free(tmp_response_buffer);
  free(http_request);
  free(data);
  free(_arg);
  tmp_response_buffer = NULL;
  http_request = NULL;
  data = NULL;
  _arg = NULL;
  return NULL;
}

POLL_ERROR_CLASS classify_poll_error(int code) {
  log_error("Poller Error: %s", strerror(code));
  switch (code) {
  case EFAULT:
    return RESET;
  case EINTR:
    return CONTINUE;
  case EINVAL:
    return RESET;
  case ENOMEM:
    return REMOVE_FD;
  default:
    return CONTINUE;
  }
}

const char *get_poll_event_description(short event) {
  switch (event) {
  case POLLIN:
    return "There is data to read.";
  case POLLPRI:
    return "There is some exceptional condition on the file descriptor.";
  case POLLOUT:
    return "Writing is now possible, though a write larger than the available "
           "space in a socket or pipe will still block (unless O_NONBLOCK is "
           "set).";
  case POLLRDHUP:
    return "Stream socket peer closed connection, or shut down writing half of "
           "connection.";
  case POLLERR:
    return "Error condition (only returned in revents; ignored in events).";
  case POLLHUP:
    return "Hang up (only returned in revents; ignored in events).";
  case POLLNVAL:
    return "Invalid request: fd not open (only returned in revents; ignored in "
           "events).";
  default:
    return "Unknown event.";
  }
}

int check_for_socket_error(int fd) {
  int err = 0;
  socklen_t len = sizeof(err);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1) {
    log_error("Error on socket %d => %s", fd, strerror(errno));
    return -1;
  }
  return err;
}
