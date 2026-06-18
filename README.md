# Concurrent Course Registration System

A multi-threaded client-server course registration system built in C using POSIX threads, TCP sockets, and file locking. The system supports concurrent access by students, faculty, and administrators while maintaining data consistency through synchronization mechanisms.

## Features

### Student Operations

* Login authentication
* View available courses
* Enroll in courses
* Drop registered courses
* View enrolled courses
* Change password

### Faculty Operations

* Login authentication
* View assigned courses
* Manage course information
* View enrolled students
* Change password

### Administrator Operations

* Add, modify, and remove students
* Add, modify, and remove faculty
* Create and manage courses
* Assign faculty to courses
* View system records

## System Design

The application follows a client-server architecture.

```text
+-----------+        TCP Socket        +-----------+
|  Client   | <---------------------> |  Server   |
+-----------+                         +-----------+
                                            |
                                            |
                                     POSIX Threads
                                            |
                                            |
                                     File Storage
                                            |
                                     fcntl Locks
```

### Concurrency Model

* The server accepts multiple client connections concurrently.
* A dedicated POSIX thread is created for each client session.
* Shared records are protected using file-region locking (`fcntl`).
* Multiple users can interact with the system simultaneously without corrupting data.

## Technologies Used

* C
* POSIX Threads (pthreads)
* TCP/IP Sockets
* fcntl File Locking
* poll()
* Linux System Calls
* File-based Persistent Storage

## Key Concepts Demonstrated

### Networking

* TCP client-server communication
* Socket programming
* Connection management

### Concurrent Systems Programming

* Multi-threaded server design
* Thread lifecycle management
* Synchronization and race-condition prevention

### Operating Systems

* File descriptors
* System calls (`read`, `write`)
* Record locking with `fcntl`
* Signal handling

### I/O Multiplexing

* Client-side timeout handling using `poll()`

## Data Storage

The system uses persistent file-based storage for:

* Student records
* Faculty records
* Course records
* Enrollment information

Record-level locking ensures consistency during concurrent updates.

## Build Instructions

```bash
make
```



## Running the Application

Start the server:

```bash
./server
```

Start the client:

```bash
./client
```

Multiple clients can be launched simultaneously to test concurrent access.

## Learning Outcomes

This project was built to gain hands-on experience with:

* Concurrent server development
* POSIX threads
* TCP socket programming
* Synchronization mechanisms
* File locking
* Linux system programming
* Multi-user system design

---

### Resume Summary

Built a concurrent client-server course registration system in C using POSIX threads, TCP sockets, and `fcntl` file locking, enabling multiple users to safely access and modify shared records while preventing race conditions through record-level synchronization.

This version makes the project sound much closer to a systems project than a typical college management application.
