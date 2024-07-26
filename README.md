# Epoll-based HTTP Server

## Overview
This project implements a high-performance HTTP server using the epoll I/O event notification mechanism. The server is capable of handling multiple concurrent connections efficiently, supporting basic HTTP methods and file serving capabilities.

## Key Features
- Utilizes epoll for efficient I/O multiplexing
- Supports GET and POST HTTP methods
- Implements custom endpoints (/ping, /echo, /read, /write, /stats)
- Serves static files
- Supports multiple concurrenct downloads for large files (>= 1GB)
- Maintains server statistics

## Skills Demonstrated
- Systems programming in C
- Network programming (sockets, HTTP protocol)
- Concurrent programming using epoll
- File I/O operations
- Memory management and buffer handling
- Error handling and logging
- Performance optimization for high-concurrency scenarios

## Getting Started
1. Clone the repository
2. Compile the server: `make all`
3. Create a `port.txt` file with your desired port number
4. Run the server: `./http_server`

## Usage
The server responds to the following endpoints:
- `/ping`: Returns "pong"
- `/echo`: Echoes back the request headers
- `/read`: Returns previously saved data
- `/write`: Saves POST data (up to 1024 bytes)
- `/stats`: Returns server statistics
- Any other path: Attempts to serve a file from the current directory

## Future Improvements
- Implement SSL/TLS support
- Add support for additional HTTP methods
- Implement a thread pool for handling requests
- Enhance logging and error reporting

## Contributing
Contributions are welcome! Please feel free to submit a Pull Request.
