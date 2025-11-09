# Wind Breaker
![CI](https://github.com/diderikk/wind_breaker/actions/workflows/ci.yml/badge.svg)

## Introduction
This is a hobby project implemented by (diderikk)[https://github.com/diderikk]. 
Wind Breaker is a HTTP server written and implemented in C. Currently, it hosts my portfolio web page, which can be found on https://diderikk.dev. The implementation has gone through countless iterations, where strategy and implementation has changed numerous times. 

The server uses mainly two data structures for handling requests. **poll_array** and **session**. Both uses the socket file descriptor as the unique identifier. 

* **poll_array** is a list of file descriptors that the server is expecting an event from. It used by the event handling thread that listenes on the TCP port. See implementation in [listener.c](src/listener.c). Since this structure is only used by a single thread all its functions are synchronous. 
* **session** is a FIFO queue used to maintain the states of all open file descriptors. It is used by all workers to fetch their next task and, therefore, is asynchronously implemented. 
* **marked_fds** is a list of failed file descriptors that must be removed from both poll_array and session.



## Functionality

### Asynchronous request handling (multithreaded)
A single thread is used for listening on the TCP port. Worker threads handle request after it has been read. 
1. On the first POLLIN event of a socket's file descriptor, the socket is added to the poll_array and session. 
2. On the second POLLIN event, the listening thread reads the request. 
3. After succesfully handling these events the worker threads each performs their task, in order, to create the response.  

TODO - State diagram

The sender performs an optimistic send of the response. If this fails, the response is stored and will be sent again on a POLLOUT event.

### TLS/HTTPS
* Uses OpenSSL for initializing and handling TLS/HTTPS
* Can handle both HTTPS and HTTP when enabling HTTPS

### HTTP Protocol
* Minimally implemented the HTTP 1.1 protocol.
* Main focus -> to validate all input.

### Static memory
All memory used by the application is allocated at the start of the application (intentional). The size of this depends on the properties used.


## TODO
* State diagram
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
