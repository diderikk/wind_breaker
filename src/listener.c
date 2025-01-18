#define _GNU_SOURCE
#include "listener.h"
#include "data_structures/poll_array.h"
#include "data_structures/session.h"
#include "shutdown/stop.h"
#include "static.h"
#include "utils/assert2.h"
#include "utils/logger.h"
#include "worker.h"
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

void _listen(int listener, void (*request_handler)(int, const char *));
void handle_request_async(int fd, const char *raw_request);
void *worker_function(void *_arg);
POLL_ERROR_CLASS classify_poll_error(int code);
const char *get_poll_event_description(short event);
int check_for_socket_error(int fd);

void listen_async(int listener) { _listen(listener, handle_request_async); }

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

void _listen(int listener, void (*request_handler)(int, const char *)) {
  assert(listener > 0);
  assert(request_handler != NULL);

  char ip_str[INET6_ADDRSTRLEN], data[REQUEST_RESPONSE_MAX_SIZE];
  char loop_counter, event_count;
  int client_socket_fd, recv_return;
  sigset_t sigmask;
  struct sockaddr_storage client_addr;
  struct pollfd *poll;
  POLL_ERROR_CLASS error_class;

  loop_counter = 0;
  event_count = 0;
  memset(data, 0, REQUEST_RESPONSE_MAX_SIZE);
  memset(ip_str, 0, INET6_ADDRSTRLEN);
  sigemptyset(&sigmask);
  init_poll_array(listener);

  // Continously listen for new connections
  while (!stop()) {
    loop_counter++;
    assert(loop_counter <= 51);
    assert(get_poll_array_size() > 0);
    log_info("Number of active sockets (including listener): %d",
             get_poll_array_size());

    // https://man7.org/linux/man-pages/man2/poll.2.html
    // NULL causes the poll system call to poll until a revents
    // is updated by the kernel
    event_count =
        ppoll(get_poll_array(), get_poll_array_size(), NULL, &sigmask);

    if (event_count < 0) {
      error_class = classify_poll_error(errno);
      assert(error_class != RESET);

      continue;
    }

    // Iterate through all the file descriptors and check for events
    for (int i = 0; i < get_poll_array_size(); i++) {
      poll = get_poll_fd_by_index(i);
      assert(poll != NULL);

      // On every 50th event, validate the existing sockets:
      if (loop_counter >= 50) {
        if (check_for_socket_error(poll->fd) == -1) {
          del_from_session_sync(poll->fd);
          remove_poll_fd_by_index_sync(&i);
        }
        loop_counter = 0;
        continue;
      }

      // ERROR HANDLING
      if (poll->revents & POLLERR) {
        error_class = classify_poll_error(errno);
        assert(error_class != RESET);
        if (error_class == REMOVE_FD) {
          del_from_session_sync(poll->fd);
          remove_poll_fd_by_index_sync(&i);
          continue;
        }
      }
      if (poll->revents & POLLNVAL) {
        log_debug("%s", get_poll_event_description(POLLNVAL));
        if (check_for_socket_error(poll->fd) == -1) {
          del_from_session_sync(poll->fd);
          remove_poll_fd_by_index_sync(&i);
          continue;
        }
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
          add_to_session_sync(client_socket_fd);
          add_poll_fd_sync(client_socket_fd);
          continue;
        } else {
          log_debug("Polling for existing client to send data...");
          // Existing socket wants to send a request
          recv_return = recv_socket(poll->fd, data, REQUEST_RESPONSE_MAX_SIZE);
          if (recv_return > 0) {

            request_handler(poll->fd, data);
            continue;
          } else if (recv_return == -1 &&
                     (errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
          } else {
            // Got error or connection closed by client
            if (recv_return == 0) {
              // Connection closed
              log_trace("Socket %d hung up", poll->fd);
            }

            del_from_session_sync(poll->fd);
            remove_poll_fd_by_index_sync(&i);
            continue;
          }
        }
      }
      // Already should have read the final data => can now close the close
      // the channel This should already have happend when recv_return == 0.
      if (poll->revents & POLLHUP) {
        log_warn("%s", get_poll_event_description(POLLHUP));
        del_from_session_sync(poll->fd);
        remove_poll_fd_by_index_sync(&i);
        continue;
      }
      if (poll->revents & POLLRDHUP) {
        log_warn("%s", get_poll_event_description(POLLRDHUP));
        del_from_session_sync(poll->fd);
        remove_poll_fd_by_index_sync(&i);
        continue;
      }
    }
  }
}

void handle_request_async(int fd, const char *raw_request) {
  queue_push(fd, raw_request);
}

void *listener_worker_function(void *_arg) {
  // worker_arg *arg = (worker_arg *)_arg;
  int send_return, response_code, response_size;
  worker_data *data;
  http_request_t *http_request = malloc(sizeof(http_request_t));
  char *tmp_response_buffer = malloc(REQUEST_RESPONSE_MAX_SIZE);

  assert(tmp_response_buffer != NULL);
  assert(http_request != NULL);

  memset(tmp_response_buffer, 0, REQUEST_RESPONSE_MAX_SIZE);
  memset(http_request, 0, sizeof(http_request_t));
  response_code = 500;

  while (!stop()) {
    data = queue_pop();
    if (data == NULL) {
      continue;
    }

    add_thread_to_session(data->fd);
    log_info("Handled by worker: %lu", (unsigned long)pthread_self());
    parse_http_request(http_request, data->data);
    response_code = validate_request_headers(http_request);

    response_size = construct_response(
        response_code, http_request->uri, http_request->accept_encoding,
        http_request->if_none_match, data->data, tmp_response_buffer);

    send_return = send_socket(data->fd, data->data, response_size);
    remove_thread_from_session();
    // TODO: Can cause the session_count to be decremented twice...
    if (http_request->connection == CLOSE || send_return == -1) {
      del_from_session_sync(data->fd);
    }

    // Free worker queue data
    free(data);

    // Reset buffers
    memset(tmp_response_buffer, 0, REQUEST_RESPONSE_MAX_SIZE);
    memset(http_request, 0, sizeof(http_request_t));
    response_code = 500;
    response_size = 0;
  }

  free(tmp_response_buffer);
  free(http_request);
  free(data);
  tmp_response_buffer = NULL;
  http_request = NULL;
  data = NULL;
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
