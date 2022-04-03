/*
 *		intclient.c
 *		AUTHOR: Riley Mosca
 *	
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

//Other Imports
#include <csse2310a4.h>
#include <csse2310a3.h>
#include <tinyexpr.h>
#include "usage.h"

#define MAX_BUFFER_SIZE 100
#define DEFAULT_PORT "5142"

typedef struct {
    int verboseFlag;
    char* portNum;
    char* fileName;
} Parameters;

typedef struct {
    char* function;
    double lower;
    double upper;
    long int segments;
    long int threads;
} Data;

/****************Function Prototypes******************/
Parameters command_line_check(int argc, char** argv);
void check_file(Parameters params);
void signal_handler(void);
void check_job_lines(Parameters params);
int args_count(char* line);
void validate_args(char* jobline, int lineNum, Parameters params);
void check_port(Parameters params);
int has_spaces(char* argument, int lineNum);
int surplus_error(char* argument, int lineNum);
Data retrieve_data(char** line);
int retrieve_socket(Parameters params);
int http_request(Parameters params, int socket, Data data, int lineNum);
void calc(char* function, double lower, double upper, int segments, int threads, int verbose);
void validate_integration(int status, int socket, int lineNum, Data data, Parameters params);
void integral_calc(Data data, Parameters params);
/*****************************************************/

int main(int argc, char** argv) {
    Parameters params = command_line_check(argc, argv);
    signal(SIGPIPE, signal_handler);
    if(strcmp(params.portNum, "0") == 0) {
        params.portNum = DEFAULT_PORT;
    }
    //check_port(params);
    check_file(params);

    check_job_lines(params);
    
    
}

Parameters command_line_check(int argc, char** argv) {
    Parameters params;
    params.verboseFlag = 0;
    params.portNum = NULL;
    params.fileName = "/dev/stdin";
    argc--;
    argv++;

    if(argc > 3 || argc == 0) {
        client_usage_error();
    }

    if(strcmp(argv[0], "-v") == 0) {
        params.verboseFlag = 1;
        argc--;
        argv++;

        if(argc == 0) {
            client_usage_error();
        }
    }

    if(argc == 1) {
        params.portNum = argv[0];
    }

    if(argc == 2) {
        params.portNum = argv[0];
        params.fileName = argv[1];
    } 
    return params;
}

void check_file(Parameters params) {
    char* jobFile = params.fileName;
    FILE* file = fopen(jobFile, "r");
		
	if(!file) {
		client_readfile_error(jobFile);
	}
}

void check_job_lines(Parameters params) {
    int lineCount = 1;
    char* line;
    FILE* jobfile = fopen(params.fileName, "r");
    while((line = read_line(jobfile)) != NULL) {
        if (line[0] == '#') {
            lineCount++;
            continue;
        }

        if(args_count(line) != 5) {
            jobline_syntax(lineCount);
            lineCount++;
            continue;
        }
        validate_args(line, lineCount, params);
        lineCount++;
    }
    fclose(jobfile);
}

int args_count(char* line) {
    int numArgs = 1;
    for(int i = 0; i < strlen(line); i++) {
        if(line[i] == ',') {
            numArgs++;
        }
    }
    return numArgs;
}

void validate_args(char* jobline, int lineNum, Parameters params) {
    double lower, upper;
    int segments, threads;
    char surplus;
    char** line = split_by_commas(jobline);

    if (sscanf(line[1], "%lf %c", &lower, &surplus) != 1) {
        jobline_syntax(lineNum);
        return;
    } else if(surplus_error(line[1], lineNum)) {
        return;
    }

    if (sscanf(line[2], "%lf %c", &upper, &surplus) != 1) {
        jobline_syntax(lineNum);
        return;
    } else if(surplus_error(line[2], lineNum)) {
        return;
    }

    if(strcmp(line[1], line[2]) == 0) {
        upper_bound_error(lineNum);
        return;
    }

    if (sscanf(line[3], "%d %c", &segments, &surplus) != 1) {
        jobline_syntax(lineNum);
        return;
    } else if(surplus_error(line[3], lineNum)) {
        return;
    }

    if (sscanf(line[4], "%d %c", &threads, &surplus) != 1) {
        jobline_syntax(lineNum);
        return;
    } else if(surplus_error(line[4], lineNum)) {
        return;
    }
    
    if(!(upper > lower)) {
        upper_bound_error(lineNum);
        return;
    }

    if(segments < 1) {
        invalid_segments(lineNum);
        return;
    }
    if (segments >= INT_MAX) {
        jobline_syntax(lineNum);
        return;
    }

    if (threads >= INT_MAX) {
        jobline_syntax(lineNum);
        return;
    }
    if(threads < 1) {
        invalid_threads(lineNum);
        return;
    }

    if(segments % threads != 0) {
        integer_multiple_error(lineNum);
        return;
    }

    if(has_spaces(line[0], lineNum)) {
        return;
    }

    Data data = retrieve_data(line);
    http_request(params, retrieve_socket(params), data, lineNum);
    // double i = calc(data.function, data.lower, data.upper, data.segments, data.threads, lineNum);
    // if (i > 0.0 || i < 0.0) { 
    //     printf("Integral of f(x) = %s from %lf -> %lf is (%lf)\n", data.function, data.lower,data.upper, i);
    // }
}

int retrieve_socket(Parameters params) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(& hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; 
    int err;
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if ((err=getaddrinfo("localhost", params.portNum, &hints, &ai))) {
        freeaddrinfo(ai);
        port_connect_error(params.portNum);
    }

    if (connect(client_socket, (struct sockaddr*)ai->ai_addr, sizeof(struct sockaddr))) {
        port_connect_error(params.portNum);
    }
    return client_socket;
}

void check_port(Parameters params) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(& hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; 
    int err;
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if ((err=getaddrinfo("localhost", params.portNum, &hints, &ai))) {
        freeaddrinfo(ai);
        port_connect_error(params.portNum);
    }

    if (connect(client_socket, (struct sockaddr*)ai->ai_addr, sizeof(struct sockaddr))) {
        port_connect_error(params.portNum);
    }
    fclose(client_socket);
}

int has_spaces(char* argument, int lineNum) {
    for(int i = 0; i < strlen(argument); i++) {
        if(isspace(argument[i])) {
            whitespace_error(lineNum);
            return 1;
        }
    } 
    return 0;   
}

int surplus_error(char* argument, int lineNum) {
    for(int i = 0; i < strlen(argument); i++) {
        if(isspace(argument[i])) {
            jobline_syntax(lineNum);
            return 1;
        }
    } 
    return 0;   
}

Data retrieve_data(char** line) {
    Data data;
    double lower, upper;
    int segments, threads;

    sscanf(line[1], "%lf", &lower);
    sscanf(line[2], "%lf", &upper);
    sscanf(line[3], "%d", &segments);
    sscanf(line[4], "%d", &threads);

    data.function = line[0];
    data.lower = lower;
    data.upper = upper;
    data.segments = segments;
    data.threads = threads;
    
    return data;
}


void calc(char* function, double lower, double upper, int segments, int threads, int verbose) {
    double x; 
    double result = 0.0;
    int increment = segments / (double) threads;
    int trapProduct = (upper - lower) / (double) threads; //Trapezoidal rule product
    int tCount = 1; //thread count (for verbose mode)
    double tLower = lower;
    double tUpper = lower + increment; 

    te_variable vars[] = {{"x", &x}};
    int err_pos;
    te_expr* expr = te_compile(function, vars, 1, &err_pos);

    if(expr) {
        if(verbose) {
            for(x = lower; x <= upper; x += increment) {
                result += 2 * te_eval(expr) * trapProduct;
                
                fprintf(stdout, "thread %d:%lf->%lf:%lf\n", tCount, tLower, tUpper, result);
                tLower += increment;
                tUpper += increment;
                tCount++;
            }
        }

        if(!verbose) {
            for(x = lower; x <= upper; x += increment) {
                result += te_eval(expr);
            }
        }
    }
    fprintf(stdout, "The integral of %s from %lf to %lf is %lf\n", function, lower, upper, result);
    te_free(expr);
} 

int http_request(Parameters params, int socket, Data data, int lineNum) {
    int bufferSize = sizeof(char)*(strlen(data.function) + 25);
    char buffer[bufferSize];
    char response[5000];
    char* statusExplanation = NULL;
    int numCharsRead, responseVal;
    HttpHeader** headers = NULL;
    char* bodyStr = NULL;
    int status = 0;
    
    sprintf(buffer,"GET /validate/%s HTTP/1.1\r\n\r\n", data.function);
    ssize_t send_data = send(socket, &buffer, strlen(buffer), 0);
    ssize_t numCharactersRead = recv(socket, &response, sizeof(response), 0);
    int response_int = parse_HTTP_response(response, sizeof(response), &status, &statusExplanation, &headers, &bodyStr);
    
    //while characters are still to be received and parsed
    while(response_int == 0 ) {
        //receive the response
        numCharactersRead = recv(socket, response, sizeof(response), 0);

        //update HTTP information with status, explanation, headers and body string from given response
        response_int = parse_HTTP_response(response, sizeof(response), &status, &statusExplanation, &headers, &bodyStr);
    }

    // parse through the status, socket, current line, and the line data and command line paramaters
    // to then validate an integration request for verbose and non-verbose settings
    validate_integration(status, socket, lineNum, data, params);

    return 0; //OK request
}

void validate_integration(int status, int socket, int lineNum, Data data, Parameters params) {
    char validateIntegral[1024];
    char response[2048];

    if(status == 400) {
        bad_expression(lineNum, data.function);
        return;
    }      

    if(status == 0) {
        communication_error();
        return;
    }

    if(status == 200) {
        if(params.verboseFlag) {
            sprintf(validateIntegral, 
            "GET /integrate/%lf/%lf/%d/%d/%s HTTP/1.1\r\n"
            "X-Verbose: yes\r\n\r\n",
            data.lower, data.upper, data.segments, data.threads,
            data.function);

        } if(!params.verboseFlag) {
            sprintf(validateIntegral, 
            "GET /integrate/%lf/%lf/%d/%d/%s HTTP/1.1\r\n\r\n",
            data.lower, data.upper, data.segments, data.threads,
            data.function);
        }
    }
    int sending = send(socket, validateIntegral, sizeof(validateIntegral), 0);
    int receiving = recv(socket, &validateIntegral, sizeof(validateIntegral), 0);
    integral_calc(data, params);
}

void signal_handler(void) {
    fprintf(stderr, "intclient: communications error\n");
    exit(3);
}

void integral_calc(Data data, Parameters params) {
    calc(data.function, data.lower, data.upper, data.segments, data.threads, params.verboseFlag);
}
