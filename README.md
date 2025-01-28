# HTTP server implemented in C

## TODO
* SSL/TLS
* Caching
* SQLite
* Images referencing
* Add more parsing for more HTTP headers.
* Logging with color and fixed sizes, add LINE and func directives.
* IPv6?
* Memory optimization (based on [Andrew Kelley Practical Data Oriented Design (DoD)](https://www.youtube.com/watch?v=IroPQ150F6c) and [Handles are the better pointers](https://floooh.github.io/2018/06/17/handles-vs-pointers.html))

## Try to remember
* Clean up includes
* Use `const` where possible
* Use `static` where possible

## Requirements
* CMake
* C compiler
* zlib
* Threads
* BearSSL (for SSL/TLS) 
* OpenSSL (for ETag) (to be replaced by BearSSL)

## Testing
* Simulation testing (based on the [Tiger Style](https://github.com/tigerbeetle/tigerbeetle/blob/main/docs/TIGER_STYLE.md))
* Valgrind (memory leak testing)

## Resources
* [getaddrinfo](https://man7.org/linux/man-pages/man3/getaddrinfo.3.html)
* [Beej's Guide](https://beej.us/guide/bgnet/html/split/client-server-background.html)
* [GeeksForGeeks's Guide](https://www.geeksforgeeks.org/socket-programming-cc/)
