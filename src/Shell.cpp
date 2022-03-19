// CPSC 3500: Shell
// Implements a basic shell (command line interface) for the file system

#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
using namespace std;

#include "Shell.h"

static const string PROMPT_STRING = "NFS> ";	// shell prompt

// Mount the network file system with server name and port number in the format of server:port
void Shell::mountNFS(string fs_loc) {
	//create the socket cs_sock and connect it to the server and port specified in fs_loc
	//if all the above operations are completed successfully, set is_mounted to true

  //Parse server:port
  char* cstring = (char*)fs_loc.c_str();
  char* token = strtok(cstring, ":");
  char* tokens[2] = {0};
  int i = 0;
  while(token) {
    tokens[i++] = token;
    token = strtok(NULL, ":");
  }
  if(i != 2) {
    cerr << "Invalid command line" << endl;
    cerr << "Usage (one of the following): " << endl;
    cerr << "./nfsclient server:port" << endl;
    cerr << "./nfsclient -s <script-name> server:port" << endl;
    exit(1);
  }

  //Get address information
  addrinfo hints, *servinfo, *p;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  int rv;
  if((rv = getaddrinfo(tokens[0], tokens[1], &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      exit(1);
  }

  //Create socket and attempt to connection with socket
  for(p = servinfo; p != NULL; p = p->ai_next) {
      
      if((cs_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
        perror("socket");
        continue;
      }
      if(connect(cs_sock, p->ai_addr, p->ai_addrlen) == -1) {
        perror("connect");
        close(cs_sock);
        continue;
      }
      break; //End loop once conn
  }
  freeaddrinfo(servinfo); 
  is_mounted = true;
}

// Unmount the network file system if it was mounted
void Shell::unmountNFS() {
	// close the socket if it was mounted
  close(cs_sock);
}

// Sends the command to the server, receives response and displays it to stdout
void Shell::send_recv(string& cmd, string cmd_name) {
  
  //Send
  char* p = (char*)cmd.c_str();
  int bytes_sent = 0;
  int msg_len = strlen(p);
  while(bytes_sent < msg_len) {
    int x = write(cs_sock, (void*)p, msg_len - bytes_sent);
    if(x == -1 || x == 0) {
      perror("write");
      unmountNFS();
      exit(1); //CHANGE THIS LATER IT SHOULD NOT EXIT WITHOUT CLOSING THE LISTENING SOCKET
    }
    p += x;
    bytes_sent += x;
  }

  //Receive
  char msg[8192]; //Potential max file size < 8KB

  //If 200 OK is found
  char* start = msg; //Start of msg (after headers)
  bool succ = false; //Flag if 200 OK is found
  int mbody_len = 0; //Msg body len for conditional printing at the very end

  char* ptr = (char*)msg;
  int bytes_recv = 0; 
  int CHUNK_SIZE = 1024;
  msg_len = CHUNK_SIZE; //To be updated when length is found
  char c_len[6] = {0}; //5 digits for 7kb file + msg header
  bool len_header = false; //Flag to find length header

  while(bytes_recv < msg_len) {

      int x = read(cs_sock, (void*)ptr, msg_len - bytes_recv);
      if(x == -1 || x == 0) {
          perror("read");
          unmountNFS();
          exit(1);
      }
      ptr += x;
      bytes_recv += x;
      
      //Check for message length
      if(!len_header) {
        //Find first header line
        int i = 1;
        bool found = false;
        while(bytes_recv > 0 && i < CHUNK_SIZE) {
          if(msg[i-1] == '\r' && msg[i] == '\n') {
            found = true;
            break;
          }
          i++;
        }
        succ = strstr(msg, "200 OK") != NULL;
        bool lfound = false;
        if(!succ) { //If no success cut message off and just print header
          msg[i-1] = '\0'; //cut off \r\n from msg
          msg_len = 0;
        } else {

          //Get to beginning of Length:X line, and read until \r\n
          if(found) {
            i += 8;
            int j = 0;
            while(i < CHUNK_SIZE - 1) {
              if(msg[i] != '\r' && msg[i+1] != '\n') {
                c_len[j] = msg[i];
              } else {
                lfound = true;
                mbody_len = atoi(c_len);
                i+=2; //Get past \r\n
                break;
              }
              j++;
              i++;
            }
          }
          
          //Update message length based on amt of bytes read
          if(lfound) {
            //Check for next end of line (\r\n)
            if(i < CHUNK_SIZE-1 && msg[i] == '\r' && msg[i+1] == '\n') {
              i += 2; //i is now equal to header length
              //msg body len + header len = msg len
              msg_len = mbody_len + i;

              //Change start to the message body
              start = msg + i;
            }
            //Update since the two message headers are found
            len_header = true;
          }
        }
      }
  } 
  *ptr = '\0'; //Prevent garbage from being printed for smaller prev msgs
  //If the msg body len is > 0 or the msg was an error print buffer or cat/head print msg
  if(mbody_len || !succ || cmd_name == "cat" || cmd_name == "head")
    cout << start << endl;
  else
    cout << "success\n";
}

// Remote procedure call on mkdir
void Shell::mkdir_rpc(string dname) {
  // to implement
  string cmd = "mkdir " + dname + "\r\n";
  send_recv(cmd, "mkdir");
}

// Remote procedure call on cd
void Shell::cd_rpc(string dname) {
  // to implement
  string cmd = "cd " + dname + "\r\n";
  send_recv(cmd, "cd");
}

// Remote procedure call on home
void Shell::home_rpc() {
  // to implement
  string cmd = "home\r\n";
  send_recv(cmd, "home");
}

// Remote procedure call on rmdir
void Shell::rmdir_rpc(string dname) {
  // to implement
  string cmd = "rmdir " + dname + "\r\n";
  send_recv(cmd, "rmdir");
}

// Remote procedure call on ls
void Shell::ls_rpc() {
  // to implement
  string cmd = "ls\r\n";
  send_recv(cmd, "ls");
}

// Remote procedure call on create
void Shell::create_rpc(string fname) {
  // to implement
  string cmd = "create " + fname + "\r\n";
  send_recv(cmd, "create");
}

// Remote procedure call on append
void Shell::append_rpc(string fname, string data) {
  // to implement
  string cmd = "append " + fname + " " + data + "\r\n";
  send_recv(cmd, "append");
}

// Remote procesure call on cat
void Shell::cat_rpc(string fname) {
  // to implement
  string cmd = "cat " + fname + "\r\n";
  send_recv(cmd, "cat");
}

// Remote procedure call on head
void Shell::head_rpc(string fname, int n) {
  // to implement
  string cmd = "head " + fname + " " + to_string(n) + "\r\n";
  send_recv(cmd, "head");
}

// Remote procedure call on rm
void Shell::rm_rpc(string fname) {
  // to implement
  string cmd = "rm " + fname + "\r\n";
  send_recv(cmd, "rm");
}

// Remote procedure call on stat
void Shell::stat_rpc(string fname) {
  // to implement
  string cmd = "stat " + fname + "\r\n";
  send_recv(cmd, "stat");
}

// Executes the shell until the user quits.
void Shell::run()
{
  // make sure that the file system is mounted
  if (!is_mounted)
 	return; 
  
  // continue until the user quits
  bool user_quit = false;
  while (!user_quit) {

    // print prompt and get command line
    string command_str;
    cout << PROMPT_STRING;
    getline(cin, command_str);

    // execute the command
    user_quit = execute_command(command_str);
  }

  // unmount the file system
  unmountNFS();
}

// Execute a script.
void Shell::run_script(char *file_name)
{
  // make sure that the file system is mounted
  if (!is_mounted)
  	return;
  // open script file
  ifstream infile;
  infile.open(file_name);
  if (infile.fail()) {
    cerr << "Could not open script file" << endl;
    return;
  }

  // execute each line in the script
  bool user_quit = false;
  string command_str;
  getline(infile, command_str, '\n');
  while (!infile.eof() && !user_quit) {
    cout << PROMPT_STRING << command_str << endl;
    user_quit = execute_command(command_str);
    getline(infile, command_str);
  }

  // clean up
  unmountNFS();
  infile.close();
}


// Executes the command. Returns true for quit and false otherwise.
bool Shell::execute_command(string command_str)
{
  // parse the command line
  struct Command command = parse_command(command_str);

  // look for the matching command
  if (command.name == "") {
    return false;
  }
  else if (command.name == "mkdir") {
    mkdir_rpc(command.file_name);
  }
  else if (command.name == "cd") {
    cd_rpc(command.file_name);
  }
  else if (command.name == "home") {
    home_rpc();
  }
  else if (command.name == "rmdir") {
    rmdir_rpc(command.file_name);
  }
  else if (command.name == "ls") {
    ls_rpc();
  }
  else if (command.name == "create") {
    create_rpc(command.file_name);
  }
  else if (command.name == "append") {
    append_rpc(command.file_name, command.append_data);
  }
  else if (command.name == "cat") {
    cat_rpc(command.file_name);
  }
  else if (command.name == "head") {
    errno = 0;
    unsigned long n = strtoul(command.append_data.c_str(), NULL, 0);
    if (0 == errno) {
      head_rpc(command.file_name, n);
    } else {
      cerr << "Invalid command line: " << command.append_data;
      cerr << " is not a valid number of bytes" << endl;
      return false;
    }
  }
  else if (command.name == "rm") {
    rm_rpc(command.file_name);
  }
  else if (command.name == "stat") {
    stat_rpc(command.file_name);
  }
  else if (command.name == "quit") {
    return true;
  }

  return false;
}

// Parses a command line into a command struct. Returned name is blank
// for invalid command lines.
Shell::Command Shell::parse_command(string command_str)
{
  // empty command struct returned for errors
  struct Command empty = {"", "", ""};

  // grab each of the tokens (if they exist)
  struct Command command;
  istringstream ss(command_str);
  int num_tokens = 0;
  if (ss >> command.name) {
    num_tokens++;
    if (ss >> command.file_name) {
      num_tokens++;
      if (ss >> command.append_data) {
        num_tokens++;
        string junk;
        if (ss >> junk) {
          num_tokens++;
        }
      }
    }
  }

  // Check for empty command line
  if (num_tokens == 0) {
    return empty;
  }
    
  // Check for invalid command lines
  if (command.name == "ls" ||
      command.name == "home" ||
      command.name == "quit")
  {
    if (num_tokens != 1) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "mkdir" ||
      command.name == "cd"    ||
      command.name == "rmdir" ||
      command.name == "create"||
      command.name == "cat"   ||
      command.name == "rm"    ||
      command.name == "stat")
  {
    if (num_tokens != 2) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "append" || command.name == "head")
  {
    if (num_tokens != 3) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else {
    cerr << "Invalid command line: " << command.name;
    cerr << " is not a command" << endl; 
    return empty;
  } 

  return command;
}

