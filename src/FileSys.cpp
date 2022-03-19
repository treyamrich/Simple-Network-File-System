// CPSC 3500: File System
// Implements the file system commands that are available to the shell.

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <string>
using namespace std;

#include "FileSys.h"
#include "BasicFileSys.h"
#include "Blocks.h"

// mounts the file system
void FileSys::mount(int sock) {
  bfs.mount();
  curr_dir = 1; //by default current directory is home directory, in disk block #1
  fs_sock = sock; //use this socket to receive file system operations from the client and send back response messages
}

// unmounts the file system
void FileSys::unmount() {
  bfs.unmount();
  close(fs_sock);
}

// make a directory
void FileSys::mkdir(const char *name)
{
  size_t len_name = strlen(name);

  //Get free block number and check for errors 504 & 505
  short blk_num = checkerr_504_505(len_name);
  if(!blk_num)
    return;

  //Create sub directory 
  dirblock_t dblk;
  dblk.magic = DIR_MAGIC_NUM;
  dblk.num_entries = 0;
  for (int i = 0; i < MAX_DIR_ENTRIES; i++)
    dblk.dir_entries[i].block_num = 0;
  bfs.write_block(blk_num, (void*) &dblk);

  //Update cwd dir_entries and check for errors 502 & 506
  if(add_cwd(blk_num, name, len_name)) {
    send_msg(200);
  }
}

// switch to a directory
void FileSys::cd(const char *name)
{
  //Get curr dir blk
  dirblock_t cwdblk;
  bfs.read_block(curr_dir, (void*)&cwdblk);
  
  //Get block number for directory and check for errors 500 & 503
  dirblock_t dblk;
  short blk_num = checkerr_500_503((void*)&cwdblk, (void*)&dblk, name);
  if(!blk_num)
    return;

  curr_dir = blk_num;
  send_msg(200);
}

// switch to home directory
void FileSys::home() {
  curr_dir = 1;
  send_msg(200);
}

// remove a directory
void FileSys::rmdir(const char *name)
{
  //Get curr dir blk
  dirblock_t cwdblk;
  bfs.read_block(curr_dir, (void*)&cwdblk);
  
  //Get block number for directory and check for errors 500 & 503
  dirblock_t rmdirblk;
  short blk_num = checkerr_500_503((void*)&cwdblk, (void*)&rmdirblk, name);
  if(!blk_num)
    return;

  //Check for error 507
  if(rmdirblk.num_entries) {
    send_msg(507);
    return;
  }

  //Remove sub directory
  bfs.reclaim_block(blk_num);

  //Remove entry from curr dir
  rem_cwd((void*)&cwdblk, blk_num);

  send_msg(200);
}

// list the contents of current directory
void FileSys::ls()
{
  string output = "";

  //Get curr dir blk
  dirblock_t cwdblk;
  bfs.read_block(curr_dir, (void *) &cwdblk);
  //Display entry list
  int i = 0;
  int loop_amt = cwdblk.num_entries;
  while(i < loop_amt) {
    //Skip deleted entries or free entry spots
    if(cwdblk.dir_entries[i].block_num > 0) { 
      output += cwdblk.dir_entries[i].name;
      //Read the entry block and check if its a directory
      dirblock_t entryblk;
      bfs.read_block(cwdblk.dir_entries[i].block_num, (void *) &entryblk);
      if(is_dir((void*)&entryblk))
        output += "/";

      if(i != loop_amt - 1) {
        output += " ";
      }
    } else {
      loop_amt++; //Inc the amt of iterations
    }
    i++;
  } 
  send_msg(200, output);
}

// create an empty data file
void FileSys::create(const char *name)
{
  size_t len_name = strlen(name);

  //Get free block number and check for errors 504 & 505
  short inode_num = checkerr_504_505(len_name);
  if(!inode_num)
    return;

  //Create inode for the file
  inode_t inode;
  inode.magic = INODE_MAGIC_NUM;
  inode.size = 0;
  for(int i = 0; i < MAX_DATA_BLOCKS; i++)
    inode.blocks[i] = 0;
  bfs.write_block(inode_num, (void*) &inode);

  //Update cwd dir_entries and check for errors 502 & 506
  if(add_cwd(inode_num, name, len_name)) {
    send_msg(200);
  }
}

// append data to a data file
void FileSys::append(const char *name, const char *data)
{

  //Get curr dir blk
  dirblock_t cwdblk;
  bfs.read_block(curr_dir, (void*)&cwdblk);

  //Get inode and block number for file and check for errors 501 & 503
  inode_t inode;
  short inode_num = checkerr_501_503((void*)&cwdblk, (void*)&inode, name);
  if(!inode_num)
    return;

  //Check if total data being appended would exceed the max file size
  int len_data = strlen(data);
  if(inode.size + len_data > MAX_FILE_SIZE) {
    send_msg(508);
    return;
  }
  
  //Prepare for appending data
  append_info app;
  app.blk_index = inode.size / BLOCK_SIZE; //Starting block to ins
  int blk_offset = inode.size % BLOCK_SIZE; //Starting ins spot in block
  app.datablk_nums = new short[(len_data/BLOCK_SIZE)+1]; //Potentially created data block numbers
  app.num_datablks = 0; //Tracks the amt of data blocks created, will index into datablk_nums
  app.existing_blk = false; //Tracks if the datablk struct was read from disk
  int count = 0; //Tracks amt loop iterations and is an index for data

  //Append data
  while(count < len_data) {

    //Read existing data block if exists
    if(inode.blocks[app.blk_index] != 0 && !app.existing_blk) {
      bfs.read_block(inode.blocks[app.blk_index], (void*)&app.datablk);
      app.existing_blk = true;
    } else if(inode.blocks[app.blk_index] == 0) {
      app.existing_blk = false;
    }

    //Check if data block is full 
    if(blk_offset == BLOCK_SIZE) {
      if(my_write_block(app, count, len_data, inode.blocks[app.blk_index])) {
        return;
      }
      blk_offset = 0;
      app.blk_index++;
    } 
    else { //Add to current block if not full
      app.datablk.data[blk_offset++] = data[count++];
    }
  }
  //If a data block never got full completely
  if(my_write_block(app, count, len_data, inode.blocks[app.blk_index])) {
    return;
  }

  //If data blocks were created, write them to the inode
  int num_inode_blks = inode_numblk(inode.size);
  int loop_count = num_inode_blks + app.num_datablks;
  int n = 0; //For indexing app.datablk_nums arr
  while(num_inode_blks < loop_count)
    inode.blocks[num_inode_blks++] = app.datablk_nums[n++];
  delete [] app.datablk_nums;

  //Write to the inode for the file to disk
  inode.size += len_data;
  bfs.write_block(inode_num, (void*)&inode);
  send_msg(200);
}

// display the contents of a data file
void FileSys::cat(const char *name)
{
  head(name, MAX_FILE_SIZE);
}

// display the first N bytes of the file
void FileSys::head(const char *name, unsigned int n)
{
  string output = "";

  //Get curr dir blk
  dirblock_t cwdblk;
  bfs.read_block(curr_dir, (void*)&cwdblk);

  //Get block number for file and check for errors 501 & 503
  inode_t inode;
  short inode_num = checkerr_501_503((void*)&cwdblk, (void*)&inode, name);
  if(!inode_num)
    return;

  //Calculate amount of iterations
  unsigned int iter_amt;
  if(n >= inode.size)
    iter_amt = inode.size;
  else
    iter_amt = n;
  //Index and flag for staying within block bound
  int blk_index = 0;
  int blk_offset = 0;
  bool read_block = false;
  datablock_t datablk;
  for(unsigned int i = 0; i < iter_amt; i++) {
    //Read block if at end or very first block
    if(!read_block) {
      bfs.read_block(inode.blocks[blk_index], (void*)&datablk);
      read_block = true;
    }
    output += datablk.data[blk_offset++];
    if(blk_offset == BLOCK_SIZE) {
      read_block = false;
      blk_offset = 0;
      blk_index++;
    }
    if(i == iter_amt - 1) {
      output += "\n";
    }
  }
  send_msg(200, output);
}

// delete a data file
void FileSys::rm(const char *name)
{
  //Get curr dir blk
  dirblock_t cwdblk;
  bfs.read_block(curr_dir, (void*)&cwdblk);
  
  //Get inode block number for file and check for errors 501 & 503
  inode_t inode;
  short inode_num = checkerr_501_503((void*)&cwdblk, (void*)&inode, name);
  if(!inode_num)
    return;


  //Remove file's data blocks
  unsigned int num_blks = inode_numblk(inode.size);
  for(unsigned int i = 0; i < num_blks; i++)
    bfs.reclaim_block(inode.blocks[i]);

  //Remove file inode
  bfs.reclaim_block(inode_num);

  //Remove entry from curr dir
  rem_cwd((void*)&cwdblk, inode_num);

  send_msg(200);
}

// display stats about file or directory
void FileSys::stat(const char *name)
{
  string output = "";

  //Get curr dir blk
  dirblock_t cwdblk;
  bfs.read_block(curr_dir, (void*)&cwdblk);

  //Get block for requested file and check for error 503
  short blk_num = file_exists((void*)&cwdblk, name);  
  if(!blk_num) {
    send_msg(503);
    return;
  }

  //Read block for file
  dirblock_t entryblk;
  bfs.read_block(blk_num, (void *) &entryblk);

  if(is_dir((void*)&entryblk)) {
    output = "Directory name: " + string(name) + "/\nDirectory block: " + to_string(blk_num);
  } else {
    inode_t inode = *((inode_t*)((void*)&entryblk));
    output = "Inode block: " + to_string(blk_num);
    output += "\nBytes in file: " + to_string(inode.size);
    output += "\nNumber of blocks: " + to_string(inode_numblk(inode.size)+1);
    output += "\nFirst block: " + to_string(inode.blocks[0]);
  }
  send_msg(200, output);
}

// HELPER FUNCTIONS (optional)

// returns true if the block is a directory
bool FileSys::is_dir(void* block) {
  dirblock_t dblk = *(dirblock_t*) block;
  return dblk.magic == DIR_MAGIC_NUM;
}

// if file DNE returns -1, else returns the block number of the file.
short FileSys::file_exists(void* block, const char *name) {
  dirblock_t dblk = *(dirblock_t*) block;
  int i = 0;
  int loop_amt = dblk.num_entries;
  while(i < loop_amt) {
    if(dblk.dir_entries[i].block_num > 0) {
      if(!strcmp(dblk.dir_entries[i].name, name)) {
        return dblk.dir_entries[i].block_num;
      }
    } else {
      loop_amt++;
    }
    i++;
  }
  
  return 0;
}

// simple formula that returns the number of data blocks in an inode
unsigned int FileSys::inode_numblk(unsigned int& size) {
  unsigned int num_blks = size / BLOCK_SIZE;
  if(size % BLOCK_SIZE > 0)
    num_blks++;
  return num_blks;
}

// Checks if file exists and if the file is a directory
// Basically combines is_dir and file_exists
// Sends error corresponding error messages using send_msg()
// Reads the block into dblk parameter if file exists
// Returns dir block number if file exists
short FileSys::checkerr_500_503(void* block, void* dblk, const char *name) {
  //Get dir block number from cwd block
  short blk_num = file_exists(block, name);
  
  if(!blk_num) {
    send_msg(503);
    return 0;
  }
  bfs.read_block(blk_num, dblk);
  if(!is_dir(dblk)) {
    send_msg(500);
    return 0;
  }
  return blk_num;
}

// Basically the same as checkerr_500_503
// Checks if file exists and if the file is a file
// Sends error corresponding error messages using send_msg()
// Reads the block into inode parameter if file exists
// Returns inode block number if the file exists
short FileSys::checkerr_501_503(void* block, void* inodeblk, const char *name) {
  //Get file block number from cwd block
  short blk_num = file_exists(block, name); 
  
  if(!blk_num) {
    send_msg(503);
    return 0;
  }
  bfs.read_block(blk_num, inodeblk);
  if(is_dir(inodeblk)) {
    send_msg(501);
    return 0;
  }
  return blk_num;
}

// Checks if fname is valid and disk is not full
// Sends error corresponding error messages using send_msg()
// if fname is valid and disk not full return block number, else return 0
short FileSys::checkerr_504_505(size_t& len_name) {
  if(len_name > MAX_FNAME_SIZE) { 
    send_msg(504);
    return 0;
  }

  //Get a free block for new inode
  short blk_num = bfs.get_free_block();
  if(!blk_num) {
    send_msg(505);
  }
  return blk_num;
}

// Gets cwd dir block
// Updates the curr dir by adding the new dir entry to an empty spot
// Returns - true on success
bool FileSys::add_cwd(short blk_num, const char *name, size_t &len_name) {

  //Read curr dir block 
  dirblock_t cwdblk;
  bfs.read_block(curr_dir, (void *) &cwdblk);

  if(cwdblk.num_entries == MAX_DIR_ENTRIES) { //Check if dir is full
    bfs.reclaim_block(blk_num);
    send_msg(506);
    return false;
  }

  //Seek for an open entry spot
  int i = 0;
  while(i < MAX_DIR_ENTRIES && cwdblk.dir_entries[i].block_num != 0 ) {
    if(!strcmp(cwdblk.dir_entries[i].name, name)) { //Check if file already exists
      bfs.reclaim_block(blk_num);
      send_msg(502);
      return false;
    }
    i++;
  }

  //Initialize dir entry values
  strcpy(cwdblk.dir_entries[i].name, name);
  cwdblk.dir_entries[i].block_num = blk_num;
  cwdblk.num_entries++;

  //Write to disk
  bfs.write_block(curr_dir, (void*)&cwdblk);

  return true;
}

//Given the dir block for the cwd, removes the dir entry for file
void FileSys::rem_cwd(void* blk, short blk_num) {
  dirblock_t cwdblk = *(dirblock_t*) blk;

  int i = 0;
  while(i < MAX_DIR_ENTRIES && cwdblk.dir_entries[i].block_num != blk_num)
    i++;
  cwdblk.num_entries--;
  cwdblk.dir_entries[i].block_num = 0;
  cwdblk.dir_entries[i].name[0] = '\0';

  //Write to disk
  bfs.write_block(curr_dir, (void*)&cwdblk);
}

bool FileSys::my_write_block(append_info& app, int& count, int& len_data, short& existblk_num) {
  //If the block already existed, write to it
  if(app.existing_blk) {
    bfs.write_block(existblk_num, (void*)&app.datablk);
  } 
  else {
    //Get a free block for new data block and check if there's space
    short datablk_num = bfs.get_free_block();
    if(!datablk_num) { //If disk is full reclaim all blocks that were written
      for(int x = 0; x < app.num_datablks; x++) {
        bfs.reclaim_block(app.datablk_nums[x]);
        delete [] app.datablk_nums;
      }
      send_msg(505);
      return true;
    }
    bfs.write_block(datablk_num, (void*)&app.datablk);
    app.datablk_nums[app.num_datablks++] = datablk_num;
  }
  return false;
} 

// sends the corresponding error message given the code
void FileSys::send_msg(int code, std::string msg) {
  string final_msg;
  switch(code) {
    case 200:
      final_msg = "200 OK\r\nLength:" + to_string(msg.length()) + "\r\n\r\n" + msg;
      break;
    case 500:
      final_msg = "500 File is not a directory" + ERR_MSG;
      break;
    case 501:
      final_msg = "501 File is a directory" + ERR_MSG;
      break;
    case 502:
      final_msg = "502 File exists" + ERR_MSG;
      break;
    case 503:
      final_msg = "503 File does not exist" + ERR_MSG;
      break;
    case 504:
      final_msg = "504 File name is too long" + ERR_MSG;
      break;
    case 505:
      final_msg = "505 Disk is full" + ERR_MSG;
      break;
    case 506:
      final_msg = "506 Directory is full" + ERR_MSG;
      break;
    case 507:
      final_msg = "507 Directory is not empty" + ERR_MSG;
      break;
    case 508:
      final_msg = "508 Append exceeds maximum file size" + ERR_MSG;
  }

  //Send message
  char* p = (char*) final_msg.c_str();
  int bytes_sent = 0;
  int msg_len = strlen(p);
  while(bytes_sent < msg_len) {
    int x = write(fs_sock, (void*)p, msg_len - bytes_sent);
    if(x == -1 || x == 0) {
      perror("write");
      unmount();
      error = true; //member variable
    }
    p += x;
    bytes_sent += x;
  }
}

// returns file system flag if there is an error with the R/W
bool FileSys::getError() const {
  return error;
}