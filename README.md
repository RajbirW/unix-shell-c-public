# Custom Unix Shell (C)

A lightweight Unix-like command shell implemented in C that supports process execution, job control, pipelines, and networking utilities.

This project demonstrates core systems programming concepts including process management, signals, pipes, sockets, and memory management on Linux.

---

## Demo

```bash
mysh$ echo hello world
hello world

mysh$ sleep 5 &
[1] 12345

mysh$ ps
sleep 5 12345

mysh$ ls | wc
word count 5
character count 28
newline count 2
```

---

## Features

### Shell Functionality
- Execute external programs using `fork()` and `execvp()`
- Background job execution using `&`
- Job tracking and completion notifications
- Signal handling (`SIGINT`, `SIGCHLD`)
- Variable assignment and expansion

### Built-in Commands
- `echo`
- `ls` (supports recursion and filtering)
- `cd`
- `cat`
- `wc`
- `kill`
- `ps`

### Pipes
Supports command pipelines such as:

```bash
ls | wc
```

Pipelines are implemented using `pipe()` and `dup2()` to connect the output of one process to the input of another.

### Networking Utilities
The shell includes a simple client/server chat system:

- `start-server`
- `close-server`
- `start-client`
- `send`

The server can manage multiple clients and broadcast messages between them.

---

## Project Structure

```
src/
  mysh.c          # Main shell loop
  commands.c      # Process execution, pipes, job control
  builtins.c      # Built-in shell commands
  variables.c     # Shell variable system
  shell_io.c      # Input/output utilities
  shell_io.h
  Makefile
```

---

## Build Instructions

Compile the shell:

```bash
make
```

Clean build files:

```bash
make clean
```

---

## Run

Start the shell:

```bash
./mysh
```

Example usage:

```bash
mysh$ echo hello
mysh$ ls
mysh$ ls | wc
mysh$ sleep 10 &
mysh$ ps
```

---

## Networking Example

Start a server:

```bash
start-server 8080
```

Connect a client:

```bash
start-client 8080 127.0.0.1
```

Send a message:

```bash
send 8080 127.0.0.1 hello
```

---

## Implementation Details

### Process Execution
External commands are executed using:
- `fork()` to create a child process
- `execvp()` to run the command
- `waitpid()` to synchronize foreground processes

### Background Jobs
Background processes are tracked using a linked list that stores:
- Job ID
- Process ID
- Command string

Completed jobs are detected using a `SIGCHLD` signal handler.

### Pipes
Pipelines are implemented using:
- `pipe()` to create communication channels
- `dup2()` to redirect file descriptors
- multiple `fork()` calls for each process in the pipeline

### Signals
The shell handles:
- `SIGINT` (Ctrl+C)
- `SIGCHLD` (child process termination)

These prevent zombie processes and allow the shell to remain responsive.

---

## Technologies Used
- C
- POSIX system calls
- Linux process management
- TCP sockets
- Signals
- File descriptors

---

## Architecture

```
        +-------------+
        |   mysh.c    |
        | Shell Loop  |
        +-------------+
               |
               v
       +---------------+
       | Command Parse |
       +---------------+
        |      |      |
        v      v      v
   Builtins  Pipes  External
              |
              v
        fork() + execvp()
```

---

## License

This project is released under the MIT License.
