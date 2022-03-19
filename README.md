# 📁 Simple Network File System

## About

A **client-server NFS** using indexed block allocation, which is built on a provided virtual disk.
The disk sets up a **Superblock**, **Free block bitmap**, **Inodes** (Files/Directories) and
**Datablocks**. The client and server communicate using a **TCP socket**, and use a **protocol**. 
to communicate.

### Message Protocol

Messages must be in the following form:

**Server response**
	Response Code\r\n
	Length:X\r\n
	\r\n
	Message body

**Client command**
	command args data\r\n


### Supported commands

- `ls`: List the contents of the current directory
- `cd <directory>`: Change to a specified directory
- `home`: Switch to the home (root) directory (similar to `cd /` in Unix)
- `rmdir <directory>`: Remove a directory. The directory must be empty
- `create <filename>`: Create an empty file
- `append <filename> <data>`: Append data to an existing file
- `stat <name>`: Display information for a given file or directory
- `cat <filename>`: Display the contents of a file
- `head <filename> <n>`: Display the first `n` bytes of the file
- `rm <filename>`: Remove a file