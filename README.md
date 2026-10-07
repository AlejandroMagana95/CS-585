# CS 585 Lab: Multithreaded TCP Chatroom

A multi-user concurrent TCP chatroom application written in C (GNU11 standard) for Linux environments. The application utilizes POSIX threads (`pthread`) for handling dynamic client connections and mutual exclusion (`pthread_mutex_t`) for synchronized message broadcasting.

## Features
- **POSIX Networking**: IPv4 TCP socket server supporting address reuse (`SO_REUSEADDR`).
- **CLI Options**: Option parsing implemented via `getopt_long`.
- **Multithreading**: Dedicated worker thread per client connection.
- **Thread Safety**: Global client state synchronized using `pthread_mutex_t`.
- **Capacity Management**: Server connection limit set to 10 simultaneous clients.
- **Authentication**: Simple password-based handshake protocol.
- **Real-Time Broadcasting**: Immediate message relaying to active chatroom members.

## Project Structure
├── Makefile     # Build automation script
├── server.c     # Server implementation
├── client.c     # Client implementation
└── README.md    # Documentation
## Compilation

To compile both the server and client executables using strict GCC options (`-std=gnu11 -Wall -Wextra -pthread -O0 -g`):
bash 
make
To clean the compiled binaries:
bash
make clean
## Usage

### 1. Starting the Server
Run the server executable specifying the port and room password:
bash
./server --port 8080 --password pass1
### 2. Connecting a Client
Run the client executable specifying the target host, port, username, and password:
bash
./client --host 127.0.0.1 --port 8080 --username alex --password pass1
### 3. Messaging & Exit
- Type any text message and press `Enter` to broadcast to all connected clients.
- Type `exit` to cleanly disconnect from the server.
