# HTTP server implemented in C

## TODO
* SSL/TLS
* Caching
* SQLite
* Images referencing
* Add simulator for testing.
* Add more parsing for more HTTP headers.
* Config file

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
