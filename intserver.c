/*
 *			intserver.c
 *		AUTHOR: Riley Mosca
 *    STUDENT NUMBER: 45358195
 *	COURSE: CSSE2310 - Assignment 4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <limits.h>
#include <pthread.h>

//Other Imports
#include <csse2310a4.h>
#include <csse2310a3.h>
#include "usage.h"

#define DEFAULT_PORT "5142"

typedef struct {
    char* portNum;
    char* maxThreads;
} ServerArgs;

/******************Prototypes******************/
ServerArgs check_command_line(int argc, char** argv);
void validate_inputs(ServerArgs args, int argc);
int open_listen(const char* port);
void process_connections(int fdServer);
int http(int socket);
/**********************************************/

int main(int argc, char** argv) {
    int fd;

    ServerArgs params = check_command_line(argc, argv);
    validate_inputs(params, argc);

    if(strcmp(params.portNum, "0") == 0) {
        fd = open_listen(DEFAULT_PORT);
        fprintf(stderr, "%s\n", DEFAULT_PORT);
        fflush(stderr);
        params.portNum = DEFAULT_PORT;
    } else {
        fd = open_listen(params.portNum);
        fprintf(stderr, "%s\n", params.portNum);
        fflush(stderr);
    }
    process_connections(fd);
    http(fd);
    return 0;
}

ServerArgs check_command_line(int argc, char** argv) {
    ServerArgs args;
    args.portNum = NULL;
    args.maxThreads = NULL;
    argc--;
    argv++;
    
    //argc is neither less than 0
    //or greater than 2
    if(argc == 0) {
        server_usage_error();
    }

    //portnum and maxthreads are given
    else if(argc == 2) {
        args.portNum = argv[0];
        args.maxThreads = argv[1];
    }

    //only portnum is given
    else if(argc == 1) {
        args.portNum = argv[0];
    }

    else {
        server_usage_error(); //other error occured
    }
    return args;
}

/*
 *
 */
void validate_inputs(ServerArgs args, int argc) {
    int portNum;
    int maxThreads;
    char surplus;

    if(argc == 2) {
        sscanf(args.portNum, "%d%c", &portNum, &surplus);

        if(portNum < 0 || portNum > 65535) {
            server_usage_error();
        }
    }

    if(argc == 3) {
        sscanf(args.portNum, "%d%c", &portNum, &surplus);
        if(portNum < 0 || portNum > 65535) {
            server_usage_error();
        }

        sscanf(args.maxThreads, "%d%c", &maxThreads, &surplus);
        if(maxThreads < 1) {
            server_usage_error();
        }

        if(sscanf(args.maxThreads, "%d%c", &portNum, &surplus) != 1) {
            server_usage_error();
        }
    }

    if(sscanf(args.portNum, "%d%c", &portNum, &surplus) != 1) {
        server_usage_error();
    }

}


int open_listen(const char* port)
{
    struct addrinfo* ai = 0;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;   // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    // listen on all IP addresses

    int err;
    if((err = getaddrinfo(NULL, port, &hints, &ai))) {
        freeaddrinfo(ai);
        fprintf(stderr, "%s\n", gai_strerror(err));
        return 1;   // Could not determine address
    }

    // Create a socket and bind it to a port
    int listenfd = socket(AF_INET, SOCK_STREAM, 0); // 0 = default protocol (TCP)

    // Allow address (port number) to be reused immediately
    int optVal = 1;
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                &optVal, sizeof(int)) < 0) {
        //socket_error();
    }

    if(bind(listenfd, (struct sockaddr*)ai->ai_addr, 
                sizeof(struct sockaddr)) < 0) {
        socket_error();
    }

    if(listen(listenfd, 10) < 0) {  // Up to 10 connection requests can queue
        socket_error();
    }

    // Have listening socket - return it
    return listenfd;
}


void process_connections(int fdServer)
{
    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;

    while(1) {
        fromAddrSize = sizeof(struct sockaddr_in);

        fd = accept(fdServer, (struct sockaddr*)&fromAddr,  &fromAddrSize);
        if(fd < 0) {
            printf("OMG ERROR RUN PLS");
            socket_error();
        }
        char hostname[NI_MAXHOST];
        int error = getnameinfo((struct sockaddr*)&fromAddr, 
                fromAddrSize, hostname, NI_MAXHOST, NULL, 0, 0);
        if(error) {
            socket_error();
        }
        http(fd);
    }
}

int http(int socket) {
    char** arguments;
    int bufferSize = (25);
    char buffer[bufferSize];
    char response[bufferSize];
    int charsRead = read(socket, buffer, bufferSize);
    int maxFields = 4; //method, address, headers, body
    char* method;
    char* address;

    while(charsRead > 0) {
        arguments = split_by_char(buffer, '\n', maxFields);
    }
    
    method = arguments[0];
    address = arguments[1];

    if(strcmp(method, "POST") == 0) {
        sprintf(response, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
        write(socket, response, strlen(response));
        return -1;
    }

    if(strcmp(method, "GET") != 0) {
        close(socket);
        return -1;
    }

    if((strstr(address, "validate") == NULL) || (strstr(address, "integrate") == NULL)) {
        sprintf(response, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
        write(socket, response, strlen(response));
        return -1;
    }
    return 0; //OK request
}