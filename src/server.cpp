#include <iostream>
#include <string.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include "FileSys.h"
using namespace std;

//Parses the command and executes it based on the command name
void parse_exec(char* command, FileSys& fs);

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cout << "Usage: ./nfsserver port#\n";
        return -1;
    }
    int port = atoi(argv[1]);

    //networking part: create the socket and accept the client connection
    int ssock, csock;
    sockaddr_in serv_addr, cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //Create server socket and bind
    if((ssock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
        exit(1);
    }
    if(bind(ssock, (sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) {
        perror("connect");
        close(ssock);
        exit(1);
    }
    if(listen(ssock, 1) == -1) {
        perror("listen");
        close(ssock);
        exit(1);
    }
    FileSys fs;
    bool err = false; //Error for clean exiting a socket fail
    //Loop forever until Ctrl-C
    while(!err) {
        if((csock = accept(ssock, (sockaddr*) &cli_addr, &clilen)) == -1) {
            perror("accept");
            close(ssock);
            exit(1);
        }
        //mount the file system
        fs.mount(csock);
     
        //loop: get the command from the client and invoke the file
        //system operation which returns the results or error messages back to the clinet
        //until the client closes the TCP connection.
        bool get_cmd = true;
        while(get_cmd) {
            char buf[4096] = {0}; //Max bash line size
            int msg_len = 4096;
            int bytes_recv = 0;
            char* p = (char*)buf;
            while(bytes_recv < msg_len) {
                int x = read(csock, (void*)buf, msg_len-bytes_recv);
                //If error or client closed connection, listen for another connection
                if(x == -1 || x == 0) {
                    perror("read");
                    get_cmd = false;
                    fs.unmount();
                    break;
                }
                p += x;
                bytes_recv += x;
                //Stop reading command when \r\n is found
                if(bytes_recv > 2 && *(p-2) == '\r' && *(p-1) == '\n')
                    break;
            }
            //Parse and execute command
            if(get_cmd) 
                parse_exec(buf, fs);
            //If read/write error stop the file system
            if(fs.getError()) {
                get_cmd = false;
                err = true;
                fs.unmount();
                break;
            }
        }
    }   
    //close the listening socket
    close(ssock);

    return 0;
}

//Parses the command and executes it based on the command name
void parse_exec(char* command, FileSys& fs) {
    //Parse cmd name
    char* tokens[3]; //3 potential fields
    tokens[0] = strtok(command, " \r\n");
    
    //Check which command
    if(strcmp(tokens[0], "mkdir") == 0) {
        tokens[1] = strtok(NULL, "\r\n");
        fs.mkdir(tokens[1]);
    }
    else if (strcmp(tokens[0], "cd") == 0) {
        tokens[1] = strtok(NULL, "\r\n");
        fs.cd(tokens[1]);
    }
    else if (strcmp(tokens[0], "home") == 0) {
        fs.home();
    }
    else if (strcmp(tokens[0], "rmdir") == 0) {
        tokens[1] = strtok(NULL, "\r\n");
        fs.rmdir(tokens[1]);
    }
    else if (strcmp(tokens[0], "ls") == 0) {
        fs.ls();
    }
    else if (strcmp(tokens[0], "create") == 0) {
        tokens[1] = strtok(NULL, "\r\n");
        fs.create(tokens[1]);
    }
    else if (strcmp(tokens[0], "append") == 0) {
        tokens[1] = strtok(NULL, " ");
        tokens[2] = strtok(NULL, "\r\n");
        fs.append(tokens[1], tokens[2]);
    }
    else if (strcmp(tokens[0], "cat") == 0) {
        tokens[1] = strtok(NULL, "\r\n");
        fs.cat(tokens[1]);
    }
    else if (strcmp(tokens[0], "head") == 0) {
        tokens[1] = strtok(NULL, " ");
        tokens[2] = strtok(NULL, "\r\n");
        fs.head(tokens[1], atoi(tokens[2]));
    }
    else if (strcmp(tokens[0], "rm") == 0) {
        tokens[1] = strtok(NULL, "\r\n");
        fs.rm(tokens[1]);
    }
    else if (strcmp(tokens[0], "stat") == 0) {
        tokens[1] = strtok(NULL, "\r\n");
        fs.stat(tokens[1]);
    }
}