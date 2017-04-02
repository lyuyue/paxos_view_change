#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "message.h"
#include "constants.h"

int port = 0;
char *port_str;
char *hostfile;

time_t init_threshold = 0;
time_t progress_threshold = 20;
time_t progress_timer = 0;
time_t vc_proof_threshold = 2;
time_t vc_proof_timer = 0;
time_t vc_resend_timer = 0;
time_t cur_time = 0;

int host_n = 0;
int self_id = 0;
char self_hostname[BUF_SIZE];

int state = 0;

char vc_entry[MAX_HOST];
char prepare_entry[MAX_HOST];

char recv_buf[BUF_SIZE];

struct Message *head = NULL;

socklen_t addrlen = SOCKADDR_SIZE;
struct sockaddr_in addr[MAX_HOST];
struct sockaddr_in self_sockaddr;
struct addrinfo hints, *res, *addr_ptr; // for getaddrinfo() of other hosts

int sockfd = -1;

uint32_t last_attempted = 0;
uint32_t last_installed = 0;

uint32_t max(uint32_t a, uint32_t b) {
    if (a > b) return a;
    return b;
}

struct thread_info {
    pthread_t tid;
    struct thread_info *next;
} *thread_head;

pthread_t * get_thread_id() {
    struct thread_info *new_thread = (struct thread_info *) malloc(sizeof(struct thread_info));
    new_thread->next = thread_head->next;
    thread_head->next = new_thread;
    return &new_thread->tid;
}

void * thread_send(char *data) {
    uint32_t *type_ptr = (uint32_t *) data;
    int message_len = 0;
    if (*type_ptr == PREPARE_OK) {
        struct Prepare_OK *prepare_ok = (struct Prepare_OK *) data;
        message_len = sizeof(struct Prepare_OK);
        if (sendto(sockfd, data, message_len, 0, 
                (struct sockaddr *) &addr[prepare_ok->preinstalled % host_n], addrlen) < 0) {
            perror("ERROR send()");
        }
        free(data);
        return NULL;
    }

    if (*type_ptr == PREPARE) {
        message_len = sizeof(struct Prepare);
    } else {
        message_len = sizeof(struct View_Change);
    }

    for (int i = 0; i < host_n; i ++) {
        if (i == self_id || vc_entry[i] == 1) continue;
        if (sendto(sockfd, data, message_len, 0, (struct sockaddr *) &addr[i], addrlen) < 0) {
            perror("ERROR send()");
        }
    }
    free(data);
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
        *(line_buffer + strlen(line_buffer) - 1) = '\0';
        printf("%s\n", line_buffer);

        if (strcmp(line_buffer, self_hostname) == 0) {
            self_id = host_n;
            host_n ++;
            continue;
        }

        memset(&hints, 0, sizeof(struct addrinfo));
        if (getaddrinfo(line_buffer, port_str, &hints, &res) != 0) {
            perror("socket() error");
            return -1;
        }
            
        memcpy(&addr[host_n], res->ai_addr, SOCKADDR_SIZE);
        host_n ++;
    }

    free(line_buffer);

    printf("Self-id: %d\n", self_id);
    printf("Length of host list: %d\n", host_n);

    return 0;
}

int leader_of_installed() {
    if (self_id == last_installed % host_n) {
        return 1;
    }
    return 0;
}

int shift_to_leader_election(int view_id) {
    state = LEADER_ELECTION;
    printf("shift_to_leader_election %d\n", view_id);
    // clear vc_entry set
    if (last_attempted < view_id) bzero(&vc_entry[0], MAX_HOST);

    last_attempted = view_id;

    // construct View_Change message
    struct View_Change *vc = (struct View_Change *) malloc(sizeof(struct View_Change));
    vc->type = 2;
    vc->server_id = self_id;
    vc->attempted = last_attempted;

    time(&progress_timer);

    // send View_Change
    pthread_t *new_thread_id = get_thread_id();
    pthread_create(new_thread_id, NULL, (void *) thread_send, vc);
    return 0;
}

// simplified version, since no updates here
void shift_to_reg_leader() {
    state = REG_LEADER;
    printf("shift_to_reg_leader\n");
    // TODO:
    return;
}

// simplified version, since no updates here
void shift_to_reg_non_leader() {
    state = REG_NON_LEADER;
    printf("shift_to_reg_non_leader\n");
    return;
}

int preinstall_ready() {
    int votes = 0;
    for (int i = 0; i < host_n; i++) {
        votes += vc_entry[i];
    }

    if (votes > host_n / 2) {
        return 1;
    } 

    return 0;
}

int prepared_ready() {
    int votes = 0;
    for (int i = 0; i < host_n; i++) {
        votes += prepare_entry[i];
    }

    if (votes > host_n / 2) {
        printf("Got Prepare_OK from majority.\n");
        return 1;
    } 

    return 0;    
}

int jump_to_new_view(uint32_t view_id) {
    bzero(&vc_entry[0], MAX_HOST);
    last_installed = view_id;
    progress_threshold = init_threshold;
    printf("\n%d: Server %d is the new leader of view %d\n\n", 
        self_id, last_installed % host_n, last_installed);
    time(&progress_timer);
    return 0;
}

void set_timeout() {
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting timeout");
    }
    return;
}

int main(int argc, char* argv[]) {
    // initialization
    bzero(&addr[0], SOCKADDR_SIZE * MAX_HOST);
    bzero(&self_sockaddr, SOCKADDR_SIZE);

    thread_head = (struct thread_info *) malloc(sizeof(struct thread_info));
    thread_head->next = NULL;

    bzero(&vc_entry[0], MAX_HOST);
    bzero(&prepare_entry[0], MAX_HOST);

    gethostname(self_hostname, BUF_SIZE);

    time(&progress_timer);
    time(&vc_proof_timer);
    time(&vc_resend_timer);

    if (leader_of_installed()) {
        state = REG_LEADER;
    } else {
        state = REG_NON_LEADER;
    }

    // parse arguments
    for (int arg_itr = 1; arg_itr < argc; arg_itr ++) {
        if (strcmp(argv[arg_itr], "-p") == 0) {
            arg_itr ++;
            port_str = (char *) argv[arg_itr];
            port = atoi(port_str);
            continue;
        }

        if (strcmp(argv[arg_itr], "-h") == 0) {
            arg_itr ++;
            hostfile = argv[arg_itr];
            continue;
        }

        if (strcmp(argv[arg_itr], "-t") == 0) {
            arg_itr ++;
            init_threshold = atoi(argv[arg_itr]);
            progress_threshold = init_threshold;
            continue;
        }
    }

    printf("progress_threshold: %ld\n", progress_threshold);

    if (construct_sockaddr() != 0) {
        perror("construct_sockaddr() failure");
        return 0;
    }

    // bind sockfd to localhost
    self_sockaddr.sin_family = AF_INET;
    self_sockaddr.sin_port = htons(port);
    self_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // initialize a non-blocking socket and 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("ERROR initialize socket");
        return -1;
    }

    // set_timeout();
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    if (bind(sockfd, (struct sockaddr *) &self_sockaddr, SOCKADDR_SIZE) < 0) {
        perror("ERROR bind socket");
        return -1;
    }

    printf("\n%d: Server %d is the new leader of view %d\n\n", 
        self_id, last_installed % host_n, last_installed);

    while (1) {
        // Upon receiving message
        bzero(recv_buf, BUF_SIZE);

        if (recvfrom(sockfd, recv_buf, BUF_SIZE, 0, 
                (struct sockaddr *) NULL, NULL) < 0) {
        }

        uint32_t *type_ptr = (uint32_t *) recv_buf;
        if (*type_ptr == VIEW_CHANGE) {
            struct View_Change *vc = (struct View_Change *) recv_buf;
            printf("receive View_Change server_id: %d, attempted: %d\n", vc->server_id, vc->attempted);
            if (vc->attempted <= last_installed) continue;
            printf("last_attempted %d\n", last_attempted);
            if (vc->attempted > last_attempted) {
                if (shift_to_leader_election(vc->attempted) < 0) {
                    perror("ERROR shift_to_leader_election()");
                    return -1;
                }

                vc_entry[self_id] = 1;
                vc_entry[vc->server_id] = 1;

            } else if (vc->attempted == last_attempted && vc_entry[vc->server_id] == 0) {
                vc_entry[vc->server_id] = 1;
                if (preinstall_ready() && last_attempted > last_installed) {
                    jump_to_new_view(last_attempted);
                    progress_threshold *= 2;
                    if (leader_of_installed()) {
                        // start Prepare Phase
                        bzero(&prepare_entry[0], MAX_HOST);
                        prepare_entry[self_id] = 1;

                        struct Prepare *prepare = (struct Prepare *) malloc(sizeof(struct Prepare));
                        prepare->type = PREPARE;
                        prepare->server_id = self_id;
                        prepare->preinstalled = last_installed;
                        prepare->local_aru = 0;

                        // TODO: send
                        pthread_t *new_thread_id = get_thread_id();
                        pthread_create(new_thread_id, NULL, (void *) thread_send, prepare);
                    }
                } else {
                    if (shift_to_leader_election(vc->attempted) < 0) {
                        perror("ERROR shift_to_leader_election()");
                        return -1;
                    }
                }
            }
        };

        if (*type_ptr == VC_PROOF) {
            struct VC_Proof *vc_proof = (struct VC_Proof *) recv_buf;
            printf("VC_Proof server_id: %d, installed: %d\n", vc_proof->server_id, vc_proof->installed);
            // reset last_installed and progress_timer
            if (vc_proof->installed == last_installed) {
                if (leader_of_installed() || vc_proof->server_id == last_installed % host_n) time(&progress_timer);
            }

            if (vc_proof->installed > last_installed) {
                jump_to_new_view(vc_proof->installed);
            }
        }

        if (*type_ptr == PREPARE) {
            struct Prepare *prepare = (struct Prepare *) recv_buf;
            if (state == LEADER_ELECTION) {
                // install the view first
                last_installed = prepare->preinstalled;
                state = REG_NON_LEADER;
            }

            struct Prepare_OK *prepare_ok = (struct Prepare_OK *) malloc(sizeof(struct Prepare_OK));
            prepare_ok->type = PREPARE_OK;
            prepare_ok->server_id = self_id;
            prepare_ok->preinstalled = last_installed;
            prepare_ok->data_list = NULL;

            // TODO: send 
            pthread_t *new_thread_id = get_thread_id();
            pthread_create(new_thread_id, NULL, (void *) thread_send, prepare_ok);
        }

        if (*type_ptr == PREPARE_OK) {
            struct Prepare_OK *prepare_ok = (struct Prepare_OK *) recv_buf;
            // ignore invalid Prepare_OK
            if (prepare_ok->preinstalled != last_installed) continue;

            prepare_entry[prepare_ok->server_id] = 1;
            if (prepared_ready()) {
                shift_to_reg_leader();
            }
        }

        // if progress_timer expired, shift to leader election
        time(&cur_time);
        if (cur_time - progress_timer > progress_threshold) {
            last_attempted = max(last_attempted, last_installed) + 1;
            // shift to leader election
            if (shift_to_leader_election(last_attempted) < 0) {
                perror("ERROR shift_to_leader_election()");
                return -1;
            }
        }

        // periodically send VC_Proof, if not in LEADER_ELECTION
        if (state == LEADER_ELECTION || cur_time - vc_proof_timer < vc_proof_threshold) {
            continue;
        }

        // reset vc_proof_timer
        time(&vc_proof_timer);

        if (leader_of_installed()) {
            time(&progress_timer);
        }

        // send VC_Proof
        struct VC_Proof *vc_proof = (struct VC_Proof *) malloc(sizeof(struct VC_Proof));
        vc_proof->type = 3;
        vc_proof->server_id = self_id;
        vc_proof->installed = last_installed;

        printf("thread_send VC_PROOF view_id: %d\n", last_installed);

        pthread_t *new_thread_id = get_thread_id();
        pthread_create(new_thread_id, NULL, (void *) thread_send, vc_proof);

        // free terminated thread's info
        struct thread_info *itr = thread_head;
        while (itr->next != NULL) {
            if (pthread_kill(itr->next->tid, 0) == 0) {
                struct thread_info *tmp = itr->next;
                itr->next = itr->next->next;
                free(tmp);
            }
            if (itr->next == NULL) break;
            itr = itr->next;
        }
    }

    return 0;
}