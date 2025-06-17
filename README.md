# Custom Shell Implementation in C

A POSIX-compliant shell implementation written in C that supports various shell features including command execution, built-in commands, and advanced string parsing capabilities.

## Features

### Basic Shell Functionality

- **REPL (Read-Eval-Print Loop)**: Interactive command-line interface with `$` prompt
- **Command Execution**: Ability to execute external programs from PATH
- **Error Handling**: Proper handling of invalid commands and error messages

### Built-in Commands

- **exit**: Terminates the shell
- **echo**: Prints arguments to stdout
- **pwd**: Displays the current working directory
- **cd**: Changes the current directory
  - Supports absolute paths
  - Supports relative paths
  - Supports home directory (`~`)
- **type**: Identifies command types
  - Shows built-in commands
  - Locates executable files in PATH

### Advanced String Parsing

- **Quote Handling**:
  - Single quotes (`'`): Preserves literal string content
  - Double quotes (`"`): Allows variable expansion
- **Escape Sequences**:
  - Backslash (`\`) outside quotes
  - Backslash within single quotes
  - Backslash within double quotes (special handling for `\"` and `\\`)

## Building and Running

### Prerequisites

- CMake (version 3.10 or higher)
- C compiler (GCC recommended)
- Make

### Build Instructions

```bash
# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make
```

### Running the Shell

```bash
./your_program.sh
```

## Usage Examples

```bash
$ echo Hello World
Hello World

$ pwd
/home/user/current/directory

$ cd /usr/local
$ cd ~
$ cd ../parent
```

```bash
$ echo 'This is a single quoted string'
This is a single quoted string

$ echo "Escape sequences: \"quotes\" and \\backslash"
Escape sequences: "quotes" and \backslash
```

```bash
$ type echo
echo is a shell builtin

$ type ls
ls is /usr/bin/ls
```

## Implementation Details

The shell is implemented in C with the following key components:

- Command parsing and tokenization
- Built-in command handling
- External program execution
- PATH environment variable management
- Advanced string parsing with quote and escape sequence support

## Planned Features (TODO)

### Redirection

- **Basic Redirection**
  - [ ] Redirect stdout (`>`)
  - [ ] Redirect stderr (`2>`)
  - [ ] Append stdout (`>>`)
  - [ ] Append stderr (`2>>`)

### Autocompletion

- **Command Completion**
  - [ ] Builtin command completion
  - [ ] Executable completion
  - [ ] Multiple completions
  - [ ] Partial completions
- **Argument Completion**
  - [ ] Builtin completion with arguments
  - [ ] Missing completions

### Pipelines

- **Command Chaining**
  - [ ] Dual-command pipeline (`|`)
  - [ ] Pipelines with built-ins
  - [ ] Multi-command pipelines

### History Management

- **Interactive History**

  - [ ] The `history` builtin
  - [ ] Listing history entries
  - [ ] Limiting history entries
  - [ ] Up-arrow navigation
  - [ ] Down-arrow navigation
  - [ ] Executing commands from history

- **History Persistence**
  - [ ] Read history from file
  - [ ] Write history to file
  - [ ] Append history to file
  - [ ] Read history on startup
  - [ ] Write history on exit
  - [ ] Append history on exit

## License

This project is part of the [CodeCrafters Shell Challenge](https://app.codecrafters.io/courses/shell/overview).
