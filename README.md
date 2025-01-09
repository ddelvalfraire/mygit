# MyGit - A Simple Git-like Version Control System

A lightweight version control system implementing core Git-like functionality, designed for Unix-based systems (Linux/macOS).

## Dependencies

This project relies on Unix-specific system headers and is **not compatible with Windows**:
- `dirent.h` - Directory entry handling
- `sys/stat.h` - File status and information
- `unistd.h` - POSIX operating system API

Tested on:
- macOS 13.0+
- Ubuntu 20.04+

## Building

```bash
mkdir build
cd build
cmake ..
make
```

Note: You may need to modify the CMakeLists.txt minimum version requirement based on your system.

## Features

### Core Commands
- `init` - Creates a new repository
- `add` - Stages files for commit
- `commit` - Records changes to the repository
- `status` - Shows working tree status
- `log` - Displays commit history

### Implementation Details

The system uses SHA-1 hashing for content tracking, implementing:
- Blob objects for file content
- Tree objects for directory structure
- Commit objects for snapshots

Files are tracked using a staging area system similar to Git's index. The object storage uses a content-addressable filesystem pattern where objects are stored by their hash values.

### Usage Example
```bash
./mygit init
./mygit add file.txt
./mygit commit "Initial commit"
```