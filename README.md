# Async HTTP Server with C

## Overview
This project implements a high-performance HTTP server using the epoll I/O event notification mechanism. The server is capable of handling multiple concurrent connections efficiently, supporting basic HTTP methods and file serving capabilities.

## Goal

Through my personal projects and internships, I have had the opportunity to experiment with backend frameworks such as Express.js, FastAPI, and others. The goal of this project was to uncover how these HTTP servers work behind the scenes, how computer networks interface with the Operating System, and how Asynchronous HTTP Servers such as ones with Express.js achieve high performance and concurrency. Specifically, this project aimed to:

1. Gain a deep understanding of the low-level networking mechanisms in Unix-like operating systems.
2. Explore the epoll I/O event notification system and its role in efficient server design.
3. Implement a basic HTTP server from scratch without relying on high-level frameworks.
4. Understand the challenges and considerations in handling concurrent connections.
5. Learn about memory management and buffer handling in network programming.
6. Investigate how file I/O operations integrate with network servers.
7. Implement basic HTTP protocol handling, including parsing requests and forming responses.
8. Explore performance optimization techniques for high-concurrency scenarios.

By building this server from the ground up, I sought to bridge the gap between high-level web frameworks and the underlying system calls and mechanisms that enable their functionality. This project has provided invaluable insights into the intricacies of network programming and the design considerations for scalable server applications.

## Key Features
- Utilizes epoll for efficient I/O multiplexing
- Supports GET and POST HTTP methods
- Implements custom endpoints (/ping, /echo, /read, /write, /stats)
- Serves static files
- Supports multiple concurrenct downloads for large files (>= 1GB)
- Maintains server statistics

## Skills Demonstrated
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
- Implementing the producer/consumer concurrency model
 
## Contributing
Contributions are welcome! Please feel free to submit a Pull Request.

