# Academia – Course Registration System

A multi-threaded Course Registration System developed in C using TCP sockets, POSIX threads, and file locking mechanisms. The system follows a client-server architecture and supports three types of users: Administrator, Faculty, and Student.

The application enables concurrent course registration and management while ensuring data consistency through advisory file locking (`fcntl`).

---

## Features

### Administrator

- Add new student accounts
- Add new faculty accounts
- View all students
- View all faculty members
- Activate blocked student accounts
- Block student accounts
- Modify student passwords
- Modify faculty passwords

### Faculty

- Secure login authentication
- View courses offered by the faculty member
- Add new courses
- Remove courses from the catalog
- Update course details
  - Course name
  - Seat limit
- Change account password

### Student

- Secure login authentication
- Enroll in available courses
- Drop enrolled courses
- View enrolled courses
- Change account password

---

## Operating Systems Concepts Used

This project demonstrates several core Operating Systems concepts:

### Process Communication

- TCP/IP Socket Programming
- Client-Server Architecture

### Concurrency

- POSIX Threads (`pthread`)
- Multi-threaded server design
- Concurrent client handling

### Synchronization

- Advisory file locking using `fcntl()`
- Shared file access protection
- Prevention of race conditions

### File Management

- Persistent storage using files
- Record management
- File descriptor operations

### Signal Handling

- Graceful shutdown using `SIGINT`
- Resource cleanup before termination

---

## System Calls Used

### Networking

- `socket()`
- `bind()`
- `listen()`
- `accept()`
- `connect()`
- `send()`
- `recv()`

### File Operations

- `open()`
- `read()`
- `write()`
- `close()`
- `lseek()`
- `ftruncate()`
- `fstat()`

### Synchronization

- `fcntl()`

### Threading

- `pthread_create()`
- `pthread_detach()`

### Process and Signal Handling

- `signal()`

---

## Project Structure

```text
.
├── data
│   ├── courses.txt
│   ├── faculty.txt
│   └── students.txt
│
├── include
│   ├── common.h
│   └── utils.h
│
├── src
│   ├── client.c
│   ├── server.c
│   └── utils.c
│
├── Makefile
├── README.md
└── Report.pdf
```

---

## Data Storage

The application stores data in plain text files.

### Student Records

Format:

```text
username|password|status|course_list
```

Example:

```text
student1|pass123|1|CS101,CS102
```

Where:

- `status = 1` → Active
- `status = 0` → Blocked

---

### Faculty Records

Format:

```text
username|password|status|course_list
```

Example:

```text
prof1|secret|1|CS101,CS201
```

---

### Course Records

Format:

```text
course_id|course_name|seat_limit|current_enrollment
```

Example:

```text
CS101|Operating Systems|60|25
```

---

## Concurrency Control

Since multiple clients can access and modify records simultaneously, file locking is used to ensure consistency.

### Read Operations

Use:

```c
F_RDLCK
```

Allows multiple readers simultaneously.

### Write Operations

Use:

```c
F_WRLCK
```

Allows only one writer at a time.

### Unlocking

Use:

```c
F_UNLCK
```

Releases previously acquired locks.

This prevents:

- Lost updates
- Corrupted records
- Concurrent enrollment inconsistencies

---

## Authentication

### Administrator Credentials

```text
Username: Admin
Password: 11112222
```

### Faculty and Student Authentication

Faculty and student credentials are verified against their respective record files.

Blocked users cannot log in.

---

## Building the Project

Compile the project using the provided Makefile:

```bash
make
```

This generates:

```text
server
client
```

---

## Running the Application

### Start the Server

```bash
./server
```

Expected output:

```text
Server listening on port 8080
```

### Start a Client

```bash
./client
```

Multiple clients may be launched simultaneously from different terminals.

---

## Client–Server Workflow

```text
Client
   |
   v
Connect to Server
   |
   v
Choose Login Type
   |
   +--------+--------+
   |        |        |
   v        v        v
 Admin   Faculty   Student
   |        |        |
   v        v        v
Authenticate User
   |
   v
Role Specific Menu
   |
   v
Perform Operations
   |
   v
Logout
```

---

## Signal Handling

The server and client both support graceful termination using:

```bash
Ctrl + C
```

### Server

```text
[Server] Caught signal 2, shutting down.
```

### Client

```text
[Client] Caught signal 2, closing connection.
```

Resources and sockets are properly released before termination.

---

## Sample Operations

### Admin

- Add Student
- Add Faculty
- View Students
- View Faculty
- Activate Student
- Block Student
- Modify Passwords

### Faculty

- Add Course
- Remove Course
- Update Course Details
- View Offered Courses
- Change Password

### Student

- Enroll in Course
- Drop Course
- View Enrolled Courses
- Change Password

---

## Key Technical Highlights

- Multi-threaded server using POSIX threads
- TCP socket-based client-server communication
- Advisory file locking with `fcntl()`
- Persistent file-based storage
- Role-based authentication
- Concurrent course registration support
- Graceful signal handling
- Linux systems programming concepts

---

## Future Improvements

- Password hashing instead of plaintext storage
- Course search functionality
- Faculty-wise enrollment reports
- Student course timetable generation
- SQLite/PostgreSQL backend integration
- Audit logging
- Enhanced role-based permissions
- Improved input validation

---

## Author

**Saheem Reshi**  
Integrated M.Tech (CSE)  
International Institute of Information Technology Bangalore (IIIT-B)

### Technologies Used

- C
- POSIX Threads
- TCP Sockets
- Linux System Calls
- File Locking (`fcntl`)
- GNU Make