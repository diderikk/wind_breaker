# Wind Breaker
![CI](https://github.com/diderikk/wind_breaker/actions/workflows/ci.yml/badge.svg)

## TODO
* State diagram
* Fix session to use assert and logger
* SSL/TLS
* SQLite
    * Images referencing
* Metrics (See tcp(7) man page, and tcp_info struct) 
    * TCP_INFO
* Add more parsing for more HTTP headers.
* Memory optimization, data-oriented design (based on [Andrew Kelley Practical Data Oriented Design (DoD)](https://www.youtube.com/watch?v=IroPQ150F6c) and [Handles are the better pointers](https://floooh.github.io/2018/06/17/handles-vs-pointers.html))
* Performance optimization (assembly understanding, cache optimization?)
* A thread that listens to console input and can send commands to the server (e.g. reload config, shutdown, etc.)
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
