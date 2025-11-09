# Wind Breaker
![CI](https://github.com/diderikk/wind_breaker/actions/workflows/ci.yml/badge.svg)

## Introduction
This is a hobby project implemented by (diderikk)[https://github.com/diderikk]. 
Wind Breaker is a HTTP server written and implemented in C. Currently, it hosts my portfolio web page, which can be found on https://diderikk.dev. The implementation has gone through countless iterations, where strategy and implementation has changed numerous times. This is the final output ;). 

The server uses mainly two data structures for handling requests: **poll_array** and **session**. Both uses the socket's file descriptor as the unique identifier. 

* **poll_array** is a list of file descriptors that the server is expecting an event from. It used by the event handling thread that listenes on the TCP port. See implementation in [listener.c](src/listener.c). Since this structure is only used by a single thread all its functions are synchronous. 
* **session** is a queue used to maintain the states of all open file descriptors. This is the cornerstone for handling request since it is used by all workers to fetch their next task. Functions are implemented to handle asynchronous access using semaphores (or mutex and conds). 
* **marked_fds** is a list of failed file descriptors that must be removed from both poll_array and session.

## Functionality

### Asynchronous request handling (multithreaded)
A single thread is used for listening on the TCP port. Worker threads handle request after it has been read. 
1. On the first POLLIN event of a socket's file descriptor, the socket is added to poll_array and session. 
2. On the second POLLIN event, the listening thread reads the request. 
3. After succesfully handling these two events the worker threads in order each performs their task to create the response.  

TODO - State diagram

The workers are responsible for performing the handler functions (defined here [src/handlers](src/handlers)):
1. **parser.c** - Responsible for parsing the HTTP request buffer into a http_request_t structure, which is used by the remaining handlers.
2. **loader.c** - Responsible for loading static HTML/CSS/PNG files, data from the database and dynamically replacing variables in the HTML templates with dynamic values.  
3. **builder.c** - Responsible for building the HTTP response by combining results from previous steps. If something went wrong in previous handlers, this handler will generate a static HTML error response.
4. **sender.c** - Responsible for sending the response. 

The sender performs an optimistic send of the response. If this fails, the response is stored and will be sent again on a POLLOUT event.

### TLS/HTTPS
* Uses OpenSSL for initializing and handling TLS/HTTPS
    * Switched from send/recv to using BIO. Easier integration with OpenSSL.
* Can handle both HTTPS and HTTP when enabling HTTPS.
    * Opens a port for HTTPS (8443) and one for HTTP (8080)
* This took way longer than expected... :/

### HTTP Protocol
* Minimal implementation of the HTTP 1.1 protocol.
* Main focus -> to validate all input.
    * Ensures all files and path references are valid.

### Static memory
* Most of the memory used by the application is allocated at the start of the application (intentional). 
* The size of this depends on the properties used (session_max_size).

### SQLite
* Mainly used to host portfolio specific data.
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
* Memory optimization, data-oriented design, spacial locality (maybe not necessary) (based on [Andrew Kelley Practical Data Oriented Design (DoD)](https://www.youtube.com/watch?v=IroPQ150F6c) and [Handles are the better pointers](https://floooh.github.io/2018/06/17/handles-vs-pointers.html))
* Performance optimization (assembly understanding, cache optimization?)
* A thread/path that listens to console/http input and can send commands to the server (e.g. reload config, shutdown, etc.)
* Basic Auth

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
