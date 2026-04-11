# Wind Breaker
![CI](https://github.com/diderikk/wind_breaker/actions/workflows/ci.yml/badge.svg)

## Introduction
This is a hobby project implemented by [diderikk](https://github.com/diderikk). 
Wind Breaker is a HTTP server written and implemented in C. Currently, it hosts my portfolio web pages, which can be found on https://diderikk.dev.

The server uses mainly two data structures for handling requests: **poll_array** and **sessions**. Both uses the socket's file descriptor as the unique identifier. 

* **poll_array** is a list of file descriptors that the server is expecting an event from. It used by the event handling thread that listenes on the TCP port. See implementation in [listener.c](src/listener.c). Since this data structure is only used by a single thread all its functions are synchronous. 
* **session** is a queue used to maintain the states of all open file descriptors. This is the cornerstone for handling request since it is used by all workers to fetch their next task. Functions for this data structure are implemented to handle asynchronous access using semaphores (or mutex and conds). 
* **marked_fds** is a list of failed or completed file descriptors that must be removed from both poll_array and session.

## Functionality

### Asynchronous request handling (multithreaded)
One dedicated listener thread accepts connections and manages the poll event loop; worker threads perform request processing once request data has been read. 
The design separates I/O (listener) from more heavy work (workers) using a small (but maybe complex) state machine.

Event flow (high level)
1. When the listener observes the first POLLIN event for a socket's file descriptor, it registers the fd in poll_array and creates (or initializes) a session entry for that connection.
2. On the next POLLIN event for the same fd, the listener reads available bytes into the session's buffer and marks the session as ready for processing.
3. Once the request has been successfully read, the session is advanced into the worker pipeline where a sequence of handler stages runs to produce the response.  

Worker pipeline
- Worker threads are organized as a pipeline. Each worker thread is assigned one handler function (see [src/handlers](src/handlers)) and repeatedly pulls sessions that are in its handler's state.
- After a handler finishes its work, it advances the session to the next pipeline state (pushes the session to the next handler). This moves the session along the pipeline until the request is fully handled and the response is sent.

Handlers (in order)
1. parser.c — Parses the session buffer into an http_request_t structure used by subsequent handlers.
2. loader.c — Loads static files (HTML/CSS/PNG), fetches data from the database, and performs template variable substitution.
3. builder.c — Builds the final HTTP response from the results of previous handlers. If an earlier step failed, builder.c will generate a suitable error page/response (depends on the HTTP error code).
4. sender.c — Sends the response over the socket. 

Notes
- The listener + worker pipeline forms a small state machine: handlers advance session states and the system routes sessions to workers responsible for those states.
- The sender performs an optimistic send: if the response was not successfully sent (e.g., EAGAIN or partial write), the session is stored and is left waiting for a POLLOUT event so the listener can resume sending.

#### Session states

**Simplified session state diagram**
<img width="861" height="1161" alt="wind_breaker_state_diagram(2)" src="https://github.com/user-attachments/assets/53557ce8-2afd-4241-8179-96d5933c88cc" />


- **PROCESSING**  
  - Transient lock state used to prevent concurrent handlers from operating on the same session.  
  - Set when a worker pops a session for processing and cleared when that worker pushes the session (to another handler or back to the queue).  

- **INITIAL**  
  - Listener-created state for a newly observed POLLIN on the HTTP socket (first POLLIN).  

- **INITIAL_SSL**  
  - Same as INITIAL but for new TLS connections (may include TLS handshake steps).  

- **REQUEST_READ**  
  - Request bytes are read into the session buffer (typically after the second POLLIN). The session is ready for parsing.  

- **PARSED**  
  - The session buffer has been parsed into an http_request_t (structured request).  

- **DATA_FETCHED**  
  - Static files, DB data and template substitutions have been fetched/applied and are available for response building.  

- **READY_TO_SEND**  
  - The HTTP response bytes have been built and the session is ready for network transmission.

- **SENT**  
  - Response successfully written to the socket. Session can be cleaned up and the FD closed/removed.

- **SEND_FAILED**  
  - Optimistic send did not complete (partial write, EAGAIN, or other non-fatal error). Session is stored and the session waits for POLLOUT.  

- **REJECTED**  
  - Terminal state indicating the request will not be handled (invalid request, permission error, unrecoverable I/O). File descriptors and session resources are closed/removed.  


### TLS/HTTPS
* Uses OpenSSL for initializing and handling TLS/HTTPS
    * Switched from send/recv to using BIO. Easier integration with OpenSSL.
* Can handle both HTTPS and HTTP when enabling HTTPS.
    * Opens a port for HTTPS (8443) and one for HTTP (8080)
* This implementation took way longer than expected... :/

### HTTP Protocol
* Minimal implementation of the HTTP 1.1 protocol.
* Main focus -> to validate all input.
    * Ensures all files and path references are valid. For example see [static.c](src/static.c)

### Static memory
* Most of the memory used by the application is allocated at the start of the application (intentional). 
* The size of the memory depends on the properties used (mostly session_max_size).

### SQLite
* Mainly used to contain portfolio specific data.
* Uses a self-implemented migration system to load static data into database.
* Files are stored as a reference given by their SHA256 sum... 

## Properties
```
session_max_size = 64 (number om simultaneous sessions. increase to scale)
worker_thread_max_size = 1 (number of threads on each handler. increase to scale)
listen_backlog_max_size = 50 (backlog argument used in listen())
http_port = 8080 (HTTP port)
https_port = 8443 (HTTPS port)
env = dev (environment: dev, test, prod)
log_file = /tmp/app.log (application log file destination)
log_level = TRACE (TRACE, DEBUG, INFO, WARN, ERROR)
log_type = CONSOLE_ONLY (CONSOLE_ONLY, FILE_ONLY, CONSOLE_FILE)
cert_file = /etc/wind_breaker/cert.pem (certificate file for TLS)
private_key_file = /etc/wind_breaker/key.pem (orivate key file for TLS)
enable_https = 0 (binary value for enabling HTTPS)
```


## TODO
* Fix session to use assert and logger
* SSL/TLS
* Metrics (See tcp(7) man page, and tcp_info struct) 
    * TCP_INFO
* Add more parsing for more HTTP headers.
* Memory optimization: data-oriented design, spacial locality (maybe not necessary) (based on [Andrew Kelley Practical Data Oriented Design (DoD)](https://www.youtube.com/watch?v=IroPQ150F6c) and [Handles are the better pointers](https://floooh.github.io/2018/06/17/handles-vs-pointers.html))
* Performance optimization (spacial locality?)
* A thread/path that listens to console/http input and can send commands to the server (e.g. reload config, shutdown, etc.)
* Basic Auth
* One thread per request (Apache)
* One list per state
* ENSURE_CAPACITY use assert for checking for memory issues...

## Try to remember
* Clean up includes
* Use `const` where possible
* Use `static` where possible

## Requirements
* CMake
* C compiler
* zlib
* Threads
* OpenSSL 
* SQLite3

## Testing
* Simulation testing 
* Unit testing
* Valgrind (memory leak testing)

## Resources
* [getaddrinfo](https://man7.org/linux/man-pages/man3/getaddrinfo.3.html)
* [Beej's Guide](https://beej.us/guide/bgnet/html/split/client-server-background.html)
* [GeeksForGeeks's Guide](https://www.geeksforgeeks.org/socket-programming-cc/)
* Man pages for most high level functions used
