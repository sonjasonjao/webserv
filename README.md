# Webserv: an HTTP/1.1 server written in C++
Authors: [Sonja](https://github.com/sonjasonjao/), [Thiwanka](https://github.com/ThiwankaS), and [Johnny](https://github.com/zoni527)
## Description
The project was written to deepen our understanding of HTTP, backend programming,
nonblocking sockets, and using poll for socket IO.

Server capabilities include:
- GET, POST, and DELETE request handling
- File uploading with form data
- Config files for setup
- Listening on multiple ports
- Hostname matching
- Directory listing and autoindexing
- Routing
- Limiting allowed methods on server and route level
## Dependencies
The project has been developed using the Ubuntu clang version 12.0.1-19ubuntu3 compiler,
make, and written in the C++ 20 standard.
## Installation
Clone the repository, open the repository root folder in the terminal and then run `make`.
## Usage
To start the server using the default configuration you can simply `make run`.
The server executable takes an optional configuration file and output log filename as
arguments. Example configuration files are inside the `config_files` folder.
