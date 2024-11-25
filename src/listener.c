#define _GNU_SOURCE
#include "listener.h"
#include "session.h"
#include "string.h"
#include "utils/logger.h"
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#define WORKER_COUNT 3

void _listen(int listener, queue_t *queue,
             void (*request_handler)(void *, void *),
             request_data *request_handler_arg);
POLL_ERROR_CLASS classify_poll_error(int code);
void add_to_pfds_sync(struct pollfd *pfds[], int newfd, int *fd_count,
                      int *fd_size);
void del_from_pfds_sync(struct pollfd pfds[], int *i, int *fd_count);
const char *get_poll_event_description(short event);
int check_for_socket_error(int fd);

int get_listener_socket(char *port) {
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
  if ((return_val = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
    log_error(__FILE__, "getaddrinfo: %s\n", gai_strerror(return_val));
    return EXIT_FAILURE;
  }

  // Iterates through all possible hosts and tries to bind a socket
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((socket_fd = open_socket(p)) < 0)
      continue;

    // allows the socket to bind to an address that is in a TIME_WAIT state.
    // Allows for quick server restarts
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) ==
        -1) {
      log_error(__FILE__, "Could not bind to the same address");
      exit(EXIT_FAILURE);
    }

    if ((bind_socket(socket_fd, p)) < 0)
      continue;

    break;
  }

  freeaddrinfo(servinfo);
  servinfo = NULL;

  if (p == NULL) {
    log_error(__FILE__, "Failed to bind the socket");
    exit(EXIT_FAILURE);
  }

  // Begins listening, BACKLOG is the max amount of waiting connections
  listen_socket(socket_fd, LISTEN_BACKLOG);

  get_in_addr_str(p->ai_addr, ip_str, sizeof(ip_str));
  log_info(__FILE__, "Listening for connections on: Address: %s, Port %d",
           ip_str, get_in_addr_port(p->ai_addr));

  return socket_fd;
}

void handle_request_async(void *_arg1, void *_arg2) {
  queue_t *queue = (queue_t *)_arg1;
  request_data *arg = (request_data *)_arg2;

  log_info(__FILE__, "Adding request to worker queue. Size: %d", queue->count);

  queue_push(queue, arg);
}

void *worker_function(void *_arg) {
  worker_arg *arg = (worker_arg *)_arg;
  queue_t *queue = (queue_t *)arg->arg;
  request_data *data;
  http_request_t http_request;

  while (1) {
    data = (request_data *)queue_pop(queue);
    add_thread_to_session(data->fd);
    log_info(__FILE__, "Handled by worker: %lu", (unsigned long)pthread_self());
    handle_request(&http_request, data->data);

    size_t response_size = construct_response(&http_request, data->data);

    send_socket(data->fd, data->data, response_size, SHOULD_NOT_EXIT);
    // sleep(2);
    remove_thread_from_session();
  }

  return NULL;
}

void listen_async(int listener) {
  request_data data;
  queue_t queue;
  pthread_t *workers[WORKER_COUNT];

  if (queue_init(&queue) != 0) {
    log_error(__FILE__, "Failed to initialize queue");
  }

  if (workers_init(workers, 3, worker_function, &queue) != 0) {
    log_error(__FILE__, "Failed to initialize workers");
  }

  _listen(listener, &queue, handle_request_async, &data);
}

void _listen(int listener, queue_t *queue,
             void (*request_handler)(void *, void *),
             request_data *request_handler_arg) {
  char ip_str[INET6_ADDRSTRLEN], data[BUFFER_SIZE];
  sigset_t sigmask;
  sigemptyset(&sigmask);

  /*
   * The function is divided into 2 loops
   * First loop is the resetting loop. When the inner loop wants to reset and
   * clear all fds, it can break out Second loop (or the inner loop) continously
   * runs polling for accepting new connections and handling existing
   * connections
   */

  while (1) {
    char loop_counter = 0;
    int fd_count = 0;
    int poll_array_size = 5;
    int poll_count = 0;
    int client_socket_fd;
    struct sockaddr_storage client_addr;
    POLL_ERROR_CLASS error_class;
    int recv_return;
    struct pollfd *poll_array = malloc(sizeof *poll_array * poll_array_size);
    init_session_cache();

    poll_array[0].fd = listener;
    poll_array[0].events =
        POLLIN; // Report ready to read on incoming connection
    fd_count = 1;

    bool exit_loop = false;
    // Continously listen for new connections
    while (!exit_loop) {
      loop_counter++;
      log_info(__FILE__,
               "Poller: Number of active sockets (including listener): %d",
               fd_count);

      // https://man7.org/linux/man-pages/man2/poll.2.html
      // NULL causes the poll system call to poll until a revents
      // is updated by the kernel
      poll_count = ppoll(poll_array, fd_count, NULL, &sigmask);

      if (poll_count < 0) {
        error_class = classify_poll_error(errno);
        if (error_class != CONTINUE)
          break;
      }

      // Iterate through all the file descriptors and check for events
      for (int i = 0; i < fd_count; i++) {
        // ERROR HANDLING
        if (poll_array[i].revents & POLLERR) {
          error_class = classify_poll_error(errno);
          if (error_class == REMOVE_FD) {
            del_from_session_sync(poll_array[i].fd);
            del_from_pfds_sync(poll_array, &i, &fd_count);
            continue;
          } else {
            exit_loop = true;
            break;
          }
        }
        if (poll_array[i].revents & POLLNVAL) {
          log_debug(__FILE__, "Poller: %s",
                    get_poll_event_description(POLLNVAL));
          if (check_for_socket_error(poll_array[i].fd) == -1) {
            del_from_session_sync(poll_array[i].fd);
            del_from_pfds_sync(poll_array, &i, &fd_count);
            continue;
          }
        }
        // HANDLES NEW SOCKET EVENTS
        if (poll_array[i].revents & POLLIN) {
          // New connection wants to connect from the accept.
          if (poll_array[i].fd == listener) {

            log_debug(__FILE__, "Poller: Polling for new client to connect...");
            client_socket_fd =
                accept_socket(listener, (struct sockaddr *)&client_addr);

            if (client_socket_fd < 0)
              continue;

            get_in_addr_str((struct sockaddr *)&client_addr, ip_str,
                            sizeof(ip_str));
            log_info(__FILE__, "Poller: Client connect %s:%d", ip_str,
                     get_in_addr_port((struct sockaddr *)&client_addr));
            add_to_session_sync(client_socket_fd);
            add_to_pfds_sync(&poll_array, client_socket_fd, &fd_count,
                             &poll_array_size);
          } else {
            // Existing socket wants to send a request
            recv_return = recv_socket(poll_array[i].fd, data, BUFFER_SIZE,
                                      SHOULD_NOT_EXIT);
            if (recv_return > 0) {
              request_handler_arg->fd = poll_array[i].fd;
              strcpy(request_handler_arg->data, data);

              request_handler(queue, request_handler_arg);
            } else {
              // Got error or connection closed by client
              if (recv_return == 0) {
                // Connection closed
                log_trace(__FILE__, "Poller: socket %d hung up",
                          poll_array[i].fd);
              }

              del_from_session_sync(poll_array[i].fd);
              del_from_pfds_sync(poll_array, &i, &fd_count);
              continue;
            }
          }
        }
        // Already should have read the final data => can now close the close
        // the channel This should already have happend when recv_return == 0.
        if (poll_array[i].revents & POLLHUP) {
          log_warn(__FILE__, "Poller: %s", get_poll_event_description(POLLHUP));
          del_from_session_sync(poll_array[i].fd);
          del_from_pfds_sync(poll_array, &i, &fd_count);
          continue;
        }
        if (poll_array[i].revents & POLLRDHUP) {
          log_warn(__FILE__, "Poller: %s",
                   get_poll_event_description(POLLRDHUP));
          del_from_session_sync(poll_array[i].fd);
          del_from_pfds_sync(poll_array, &i, &fd_count);
          continue;
        }
        // On every 50th event, validate the existing sockets:
        if (loop_counter >= 50) {
          if (check_for_socket_error(poll_array[i].fd) == -1) {
            del_from_session_sync(poll_array[i].fd);
            del_from_pfds_sync(poll_array, &i, &fd_count);
          }
          loop_counter = 0;
          continue;
        }
      }
    }
    // In case an error occurs, break the loop and reset the fd array.
    for (int i = 0; i < fd_count; i++)
      close(poll_array[i].fd);
    free(poll_array);
  }
}

POLL_ERROR_CLASS classify_poll_error(int code) {
  log_error(__FILE__, "Poller Error: %s", strerror(code));
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

// https://beej.us/guide/bgnet/html/split/slightly-advanced-techniques.html
// Add a new file descriptor to the set
void add_to_pfds_sync(struct pollfd *pfds[], int newfd, int *fd_count,
                      int *fd_size) {
  // If we don't have room, add more space in the pfds array
  if (*fd_count == *fd_size) {
    *fd_size *= 2; // Double it

    *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
  }

  (*pfds)[*fd_count].fd = newfd;
  (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

  (*fd_count)++;
}

// Remove an index from the set
void del_from_pfds_sync(struct pollfd pfds[], int *i, int *fd_count) {
  log_trace(__FILE__, "Poller: Removing fd %d", pfds[*i].fd);
  close(pfds[*i].fd);
  // Copy the one from the end over this one
  pfds[*i] = pfds[*fd_count - 1];

  (*fd_count)--;
  (*i)--;
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
    log_error(__FILE__, "Poller: Error on socket %d => %s", fd,
              strerror(errno));
    return -1;
  }
  return err;
}
