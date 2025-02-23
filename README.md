# Wind Breaker
![CI](https://github.com/diderikk/wind_breaker/actions/workflows/ci.yml/badge.svg)

## TODO
* Stack trace
* Fix session to use assert and logger
* Add insert_time in worker_data
* SSL/TLS
* Caching
* SQLite
* Images referencing
* Add more parsing for more HTTP headers.
* Memory optimization, data-oriented design (based on [Andrew Kelley Practical Data Oriented Design (DoD)](https://www.youtube.com/watch?v=IroPQ150F6c) and [Handles are the better pointers](https://floooh.github.io/2018/06/17/handles-vs-pointers.html))
* Performance optimization (assembly understanding, cache optimization?)

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

## Testing
* Simulation testing (based on the [Tiger Style](https://github.com/tigerbeetle/tigerbeetle/blob/main/docs/TIGER_STYLE.md))
* Valgrind (memory leak testing)

## Resources
* [getaddrinfo](https://man7.org/linux/man-pages/man3/getaddrinfo.3.html)
* [Beej's Guide](https://beej.us/guide/bgnet/html/split/client-server-background.html)
* [GeeksForGeeks's Guide](https://www.geeksforgeeks.org/socket-programming-cc/)
