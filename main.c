#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "message.h"
#include "constants.h"

int port = 0;
char *port_str;
char *hostfile;

int host_n = 0;
int self_id = 0;
char self_hostname[BUF_SIZE];

struct Message *head = NULL;

struct sockaddr_in addr[MAX_HOST];
struct addrinfo hints, *res, *addr_ptr; // for getaddrinfo() of other hosts


void *thread_send() {
    return NULL;
}

int construct_sockaddr() {
    host_n = 0;

    char *line_buffer = (char *) malloc(sizeof(char) * BUF_SIZE);

    FILE *fp;
    fp = fopen(hostfile, "r");
    if (fp < 0) {
        perror("ERROR invalid hostfile");
        return -1;
    }
    
    while (fgets(line_buffer, BUF_SIZE, (FILE *) fp)) {
        host_n ++;
        *(line_buffer + strlen(line_buffer) - 1) = '\0';

        if (strcmp(line_buffer, self_hostname) == 0) {
            self_id = host_n;
            continue;
        }

        memset(&hints, 0, sizeof(struct addrinfo));
        if (getaddrinfo(line_buffer, port_str, &hints, &res) != 0) {
            perror("socket() error");
            return -1;
        }
        
        memcpy(&addr[host_n], res->ai_addr, SOCKADDR_SIZE);
    }

    free(line_buffer);

    printf("Self-id: %d\n", self_id);
    printf("Length of host list: %d\n", host_n);

    return 0;
}

int main(int argc, char* argv[]) {
    // initialization
    bzero(addr, SOCKADDR_SIZE * MAX_HOST);

    // parse arguments
    for (int arg_itr = 1; arg_itr < argc; arg_itr ++) {
        if (strcmp(argv[arg_itr], "-p") == 0) {
            arg_itr ++;
            port_str = (char *) argv[arg_itr];
            port = atoi(argv[arg_itr]);
            continue;
        }

        if (strcmp(argv[arg_itr], "-h") == 0) {
            arg_itr ++;
            hostfile = argv[arg_itr];
            continue;
        }
    }

    if (construct_sockaddr() != 0) {
        perror("construct_sockaddr() failure");
        return 0;
    }


}