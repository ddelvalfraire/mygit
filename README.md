# MyGit - A Simple Git-like Version Control System

A lightweight version control system implementing core Git-like functionality, designed for Unix-based systems (Linux/macOS).

## Dependencies

This project requires the following libraries and headers:
- OpenSSL 3.4.0 (libssl-dev) - For SHA-1 hashing
- uthash 2.3.0 - For hash table implementation
- zlib 1.3.1 - For data compression

System headers (**not compatible with Windows**):
- `dirent.h` - Directory entry handling
- `sys/stat.h` - File status and information
- `unistd.h` - POSIX operating system API

Tested on:
- macOS 15.1.1

## Building

```bash
mkdir build
cd build
cmake ..
make
```

Note: You most likely need to modify the CmakeLists.txt to correctly reference your dependencies

## Features

### Core Commands
- `init` - Creates a new repository
- `add` - Stages files for commit
- `commit` - Records changes to the repository
- `status` - Shows working tree status
- `log` - Displays commit history
- `.myignore` - Supports ignoring files/directories (similar to .gitignore)

### Implementation Details

The system uses SHA-1 hashing for content tracking, implementing:
- Blob objects for file content
- Tree objects for directory structure
- Commit objects for snapshots

Files are tracked using a staging area system similar to Git's index. The object storage uses a content-addressable filesystem pattern where objects are stored by their hash values.

### Design Patterns Used
- Factory Pattern: For creating different types of objects (blobs, trees, commits)
- Singleton Pattern: For repository and index management
- Observer Pattern: For monitoring file system changes
- Strategy Pattern: For different hash computation methods

### Usage Example
```bash
./mygit init
./mygit add file.txt
./mygit commit "Initial commit"
```