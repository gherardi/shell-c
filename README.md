# shell-c

A custom implementation of a UNIX-like shell written in C.

## Description

`shell-c` is a robust, custom-built UNIX-like shell that enables direct interaction with the kernel and efficient process execution. This project focuses on the core mechanics of operating systems, including process lifecycle management, file descriptor manipulation, and low-level system calls.

The implementation features an advanced command parser capable of handling complex quote nesting and environment variable expansion. It also bridges the gap between low-level functionality and user experience by integrating command-line autocompletion and a persistent command history.

### Key Features

* **Process & Filesystem Control:** Managed low-level process execution, foreground/background task handling, and filesystem navigation.
* **I/O Redirection & Piping:** Engineered seamless data flow between processes using pipes and redirection for `stdin`, `stdout`, and `stderr`.
* **Advanced Command Parsing:** Accurate processing of complex command strings, including nested quotes and variable expansion.
* **Enhanced UX:** Integrated command autocompletion and history management for an intuitive terminal interface.

## Usage

```bash
# Start the shell
./your_program.sh

# Example of piping and redirection
cat input.txt | grep "pattern" > output.txt

# Filesystem navigation
cd /usr/src/utils

```

## Getting Started

1. **Clone the repository:** Download the source code to your local machine.
2. **Requirements:** Ensure you have a C compiler (like `gcc`) and the `readline` library installed.
