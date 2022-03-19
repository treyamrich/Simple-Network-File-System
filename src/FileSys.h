// CPSC 3500: File System
// Implements the file system commands that are available to the shell.

#ifndef FILESYS_H
#define FILESYS_H

#include "BasicFileSys.h"
#include "Blocks.h"
#include <string>

class FileSys {
  
  public:
    // mounts the file system
    void mount(int sock);

    // unmounts the file system
    void unmount();

    // make a directory
    void mkdir(const char *name);

    // switch to a directory
    void cd(const char *name);
    
    // switch to home directory
    void home();
    
    // remove a directory
    void rmdir(const char *name);

    // list the contents of current directory
    void ls();

    // create an empty data file
    void create(const char *name);

    // append data to a data file
    void append(const char *name, const char *data);

    // display the contents of a data file
    void cat(const char *name);

    // display the first N bytes of the file
    void head(const char *name, unsigned int n);

    // delete a data file
    void rm(const char *name);

    // display stats about file or directory
    void stat(const char *name);

    // returns file system flag if there is an error with the R/W
    bool getError() const;

  private:
    BasicFileSys bfs;	// basic file system
    short curr_dir;	// current directory

    int fs_sock;  // file server socket

    // Additional private variables and Helper functions - if desired
    bool error = false; //Used to clean exit the listening socket on socket failure

    const std::string ERR_MSG = "\r\nLength:0\r\n\r\n"; //append to error messages

    struct append_info { //helper struct to pass arguments to function append()
      int blk_index;
      short* datablk_nums;
      int num_datablks;
      bool existing_blk;
      datablock_t datablk;
    };

    // returns true if the block is a directory
    bool is_dir(void* block); 

    // if file DNE returns 0, else returns the block number of the file.
    short file_exists(void* block, const char *name);

    // simple formula that returns the number of data blocks in an inode
    unsigned int inode_numblk(unsigned int& size);

    // Checks if file exists and if the file is a directory
    // Basically combines is_dir and file_exists
    // Sends error corresponding error messages using send_msg()
    // Reads the block into dblk parameter if file exists
    // Returns dir block number if file exists
    short checkerr_500_503(void* block, void* dblk, const char *name);

    // Basically the same as checkerr_500_503
    // Checks if file exists and if the file is a file
    // Sends error corresponding error messages using send_msg()
    // Reads the block into inode parameter if file exists
    // Returns inode block number if the file exists
    short checkerr_501_503(void* block, void* inode, const char *name);

    // Checks if fname is valid and disk is not full
    // Sends error corresponding error messages using send_msg()
    // if fname is valid and disk not full returns free block number, else return 0
    short checkerr_504_505(size_t& len_name);

    // Gets cwd dir block
    // Updates the curr dir by adding the new dir entry to an empty spot
    // Returns - true on success
    bool add_cwd(short blk_num, const char *name, size_t& len_name);

    // Given the dir block for the cwd, removes the dir entry for file
    void rem_cwd(void* blk, short blk_num);

    // Fat helper function to write a data block that already exists,
    // or to a new block. append_info is just to pass more variables and,
    // reduce the argument amounts. Returns false if no errors occur.
    bool my_write_block(append_info& app, int& count, int& len_data, short& existblk_num);

    // sends the corresponding error message given the code
    // returns true if the socket write is a success, false otherwise
    void send_msg(int code, std::string msg="");
};

#endif