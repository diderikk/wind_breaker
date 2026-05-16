#define _GNU_SOURCE
#include "listener.h"
#include "data_structures/marked_fds.h"
#include "data_structures/poll_array.h"
#include "data_structures/session.h"
#include "shutdown/stop.h"
#include "socket.h"
#include "utils/assert2.h"
#include "utils/logger.h"
#include <errno.h>
#include <openssl/err.h>

extern int count;
extern struct pollfd *fds;
POLL_ERROR_CLASS classify_poll_error(int code);
const char *get_poll_event_description(short event);
int check_for_socket_error(int fd);

void _listen(int listener, int listener_ssl, int (*request_handler)(int));

int handle_request_async(int fd) {
  int recv_return;
  session_t *session = pop_session_for_read(fd);
  if (session == NULL) {
    log_debug("Session is NULL for fd %d", fd);
    return -1;
  }
  assert(session->bio != NULL);

  if (session->request.is_ssl) {
    log_debug("Reading SSL request for fd %d", fd);
    assert(session->ssl != NULL);
    // Alloc more space for the data
    recv_return = recv_ssl(session->ssl, session->buffer);
    if (recv_return > 0) {
      push_session(fd, WORK_STATUS_REQUEST_READ);
      return 0;
    } else if (recv_return <= 0 && SSL_get_error(session->ssl, recv_return) ==
                                       SSL_ERROR_WANT_READ) {
      push_session(fd, WORK_STATUS_INITIAL);
      return 0;
    } else {
      // Got error or connection closed by client
      if (recv_return == 0) {
        // Connection closed
        log_trace("Socket %d hung up", fd);
      }
      return -1;
    }
  } else {
    log_debug("Reading plain request for fd %d", fd);
    recv_return = recv_bio(session->bio, session->buffer);
    if (recv_return > 0) {
      push_session(fd, WORK_STATUS_REQUEST_READ);
      return 0;
    } else if (recv_return <= 0 && BIO_should_retry(session->bio)) {
      push_session(fd, WORK_STATUS_INITIAL);
      return 0;
    } else {
      log_error("BIO read error");
      // Got error or connection closed by client
      if (recv_return == 0) {
        // Connection closed
        log_trace("Socket %d hung up", fd);
      }
      return -1;
    }
  }
}

void listen_async(int listener, int listener_ssl) {
  _listen(listener, listener_ssl, handle_request_async);
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

void _listen(int listener, int listener_ssl, int (*request_handler)(int)) {
  assert(listener > 0);
  assert(request_handler != NULL);

  char ip_str[INET6_ADDRSTRLEN];
  char event_count;
  int client_socket_fd, new_conn_ret;
  sigset_t sigmask;
  struct sockaddr_storage client_addr;
  struct pollfd *poll;
  POLL_ERROR_CLASS error_class;

  event_count = 0;
  memset(ip_str, 0, INET6_ADDRSTRLEN);
  sigemptyset(&sigmask);

  // Continously listen for new connections
  while (!is_shutdown_requested()) {
    assert(count > 0);
    log_info("Number of active sockets (including listener): %d", count);

    // https://man7.org/linux/man-pages/man2/poll.2.html
    // NULL causes the poll system call to poll until a revents
    // is updated by the kernel
    event_count = ppoll(fds, count, NULL, &sigmask);

    if (event_count < 0) {
      error_class = classify_poll_error(errno);
      assert(error_class != RESET);

      continue;
    }

    // Iterate through all the file descriptors and check for events
    for (int i = 0; i < count; i++) {
      poll = &fds[i];
      assert(poll != NULL);

      if (is_marked(poll->fd)) {
        log_debug("Socket %d is marked, removing from poll array...", poll->fd);
        remove_poll_fd_by_index_sync(&i);
        continue;
      }

      if (check_for_socket_error(poll->fd) == -1) {
        log_debug("Socket %d is invalid, removing...", poll->fd);
        push_session(poll->fd, WORK_STATUS_REJECTED);
        remove_poll_fd_by_index_sync(&i);
        continue;
      }

      // ERROR HANDLING
      if (poll->revents & POLLERR) {
        error_class = classify_poll_error(errno);
        assert(error_class != RESET);
        if (error_class == REMOVE_FD) {
          log_debug("Socket %d error event, removing...", poll->fd);
          push_session(poll->fd, WORK_STATUS_REJECTED);
          remove_poll_fd_by_index_sync(&i);
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
          push_session(poll->fd, WORK_STATUS_REJECTED);
          remove_poll_fd_by_index_sync(&i);
          continue;
        }
      }
      if (poll->revents & POLLOUT) {
        log_debug("Socket %d is ready for writing", poll->fd);
        push_session(poll->fd, WORK_STATUS_READY_TO_SEND);

        continue;
      }
      // HANDLES NEW SOCKET EVENTS
      if (poll->revents & POLLIN) {
        // New connection wants to connect from the accept.
        if (poll->fd == listener || poll->fd == listener_ssl) {

          log_debug("Polling for new client to connect...");
          client_socket_fd =
              accept_socket(poll->fd, (struct sockaddr *)&client_addr);

          if (client_socket_fd < 0)
            continue;

          get_in_addr_str((struct sockaddr *)&client_addr, ip_str,
                          sizeof(ip_str));
          if (poll->fd == listener) {
            log_debug("New client connected on HTTP socket %s:%d", ip_str,
                      get_in_addr_port((struct sockaddr *)&client_addr));
            push_session(client_socket_fd, WORK_STATUS_INITIAL);
          } else {
            log_debug("New client connected on HTTPS socket %s:%d", ip_str,
                      get_in_addr_port((struct sockaddr *)&client_addr));
            push_session(client_socket_fd, WORK_STATUS_INITIAL_SSL);
          }
          int poll_array_index = add_poll_fd_sync(client_socket_fd);
          continue;
        } else {
          log_debug("Polling for existing fd %d to send data...", poll->fd);

          if (request_handler(poll->fd) == -1) {
            push_session(poll->fd, WORK_STATUS_REJECTED);
            remove_poll_fd_by_index_sync(&i);
          }
        }
      }
    }
  }
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
    return REMOVE_FD;
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
