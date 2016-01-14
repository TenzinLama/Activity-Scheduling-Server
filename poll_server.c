#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ctype.h>
#include <errno.h>
#include "lists.h"

#define DELIM " \n"
#ifndef PORT
#define PORT 11447
#endif
#define MAXNAME 32
#define MAXINPUT 256
#define MAXCLIENT 5
#define INPUT_ARG_MAX_NUM 12
static int listenfd;

struct client{
    int fd;
    struct in_addr ipaddr;
    struct client *next;
    char name[MAXNAME];
    char input[MAXINPUT];
    int inbuf;
    int room;
    char *after;
} *top = NULL;
//create the heads of the empty data structure
Poll *poll_list = NULL;

static void addclient(int fd, struct in_addr addr);
static void removeclient(int fd);
static void bindandlisten();
static char *read_client_input(struct client *p);
static int execute_poll_commands(char *input, struct client *p);
static int process_args(int cmd_argc, char **cmd_argv, Poll **poll_list_ptr, struct client *p);
static char announcement[] = "There has been activity in this poll\r\n";
static char confirmation[] = "Go ahead and enter poll command\r\n";
static int num_clients();

void error(char *msg){
    fprintf(stderr, "Error: %s\n", msg);
}

int main(){
    struct client *p;
    extern void bindandlisten(), newconnection();
    
    bindandlisten();
    
    while(1){
        fd_set fdlist;
        int maxfd = listenfd;
        FD_ZERO(&fdlist);
        FD_SET(listenfd, &fdlist);
        //set the largest fd
        for(p = top; p; p = p->next){
            FD_SET(p->fd, &fdlist);
            if(p->fd > maxfd){
                maxfd = p->fd;
            }
        }
        
        if (select(maxfd + 1, &fdlist, NULL, NULL, NULL) < 0){
            perror("select");
            exit(1);
        }
        
        for(p = top; p; p = p->next){
            
            if(FD_ISSET(p->fd, &fdlist)){
                
                char *client_input = read_client_input(p);
                if(client_input != NULL){
                    printf("input from %s\n", p->name);
                    execute_poll_commands(client_input, p);
                }
            }
        }
        if (FD_ISSET(listenfd, &fdlist)){
            newconnection();
        }
    }
    
    return 0;
}

void bindandlisten(){
    struct sockaddr_in r;
    
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(1);
    }
    int on = 1;
    int status = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
    (const char *) &on, sizeof(on));
    if(status == -1) {
        perror("setsockopt -- REUSEADDR");
    }
    
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);
    
    if (bind(listenfd, (struct sockaddr *)&r, sizeof(r))){
        perror("bind");
    }
    
    if(listen(listenfd, MAXCLIENT)){
        perror("listen");
    }
    
}
//code from example server
//setup a connection from client
void newconnection(){
    int fd;
    struct sockaddr_in r;
    socklen_t socklen = sizeof(r);
    char buf[30];
    
    if((fd = accept(listenfd, (struct sockaddr *)&r, &socklen)) < 0){
        perror("accept");
    } else {
        printf("connection from %s\n", inet_ntoa(r.sin_addr));
        addclient(fd, r.sin_addr);
        struct client *p;
        for(p = top; p; p = p->next){
            if(p->fd == fd){
                break;
            }
        }
        sprintf(buf, "What is your username?\r\n");
        if(write(p->fd, buf, strlen(buf)) == -1){
            perror("write fail");
            removeclient(p->fd);
        }
    }
}

static void addclient(int fd, struct in_addr addr){
    struct client *p = malloc(sizeof(struct client));
    
    if(!p){
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    
    printf("Adding client %s\n", inet_ntoa(addr));
    fflush(stdout);
    
    p->fd = fd;
    p->ipaddr = addr;
    p->inbuf = 0;
    p->room = sizeof(p->input);
    p->after = p->input;
    memset(p->input, '\0', sizeof(p->input));
    p->next = top;
    top = p;
}

static void removeclient(int fd){
    
    struct client *prev_head = NULL;
    struct client *client_to_delete;
    struct client *client_to_delete_next;
    struct client *client_iter = top;
    //find the client to delete's next client
    struct client *iter = top;
    while(iter != NULL){
        if(iter->fd == fd){
            client_to_delete = iter;
            client_to_delete_next = iter ->next;
            break;
        }
        iter = iter->next;
    }
    if(close(client_to_delete->fd) == -1){
        perror("closing client file descriptor");
        exit(1);
    }
    
    while(client_iter->next != NULL){
        if((client_iter->next)->fd == fd){
            prev_head = client_iter;
            break;
        }
        client_iter = client_iter -> next;
    }
    
    if(prev_head != NULL){
        prev_head->next = client_to_delete_next;
        printf("removing client %s we now have %d clients\n", client_to_delete->name, num_clients());
    }
    
    else if(prev_head == NULL){
        top = client_to_delete->next;
        printf("removing client %s we now have %d clients\n", client_to_delete->name, num_clients());
    }
    
    else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", fd);
        fflush(stderr);
    }
}

static void broadcast(char *s, int size, Poll *poll){
    //broadcast to all participants in the same poll to notify activity
    struct client *p;
    for(p = top; p; p = p->next){
        if(find_part(p->name, poll) != NULL){
            if(write(p->fd,s,size) == -1){
                perror("write fail");
                removeclient(p->fd);
            }
        }
    }
}

int find_network_newline(char *buf, int inbuf){
    int i;
    for(i = 0; i < inbuf; i++){
        if(buf[i] == '\n'){
            return i;
        }
    }
    return -1;
}


char *read_client_input(struct client *p){
    
    char *input = malloc(MAXINPUT);
    int len;
    input[0] = '\0';
    int where;
    
    if (strlen(p->name) != 0 ){
        if((len = read(p->fd, p->after, p->room)) == -1){
            perror("read");
            removeclient(p->fd);
            return NULL;
        }
        p->inbuf += len;
        where = find_network_newline(p->input, p->inbuf);
        
        if(where >= 0){
            
            input[where] = '\n';
            input[where+1] = '\0';
            strcat(input, p->input);
            
            p->inbuf -= where + 2;
            if(p->inbuf > 0){
                memmove(p->input, &((p->input)[where + 2]), p->inbuf);
            } else {
                memset(p->input, '\0', sizeof(p->input));
                p->inbuf = 0;
            }
            p->after = &((p->input)[p->inbuf]);
            return input;
        }
        
        p->room = sizeof(p->input) - (p->inbuf);
        p->after = &((p->input)[p->inbuf]);
        return NULL;
    }
    else{
        if((len = read(p->fd, p->after, p->room)) == -1){
            perror("read");
            removeclient(p->fd);
            return NULL;
        }
        p->inbuf += len;
        
        where = find_network_newline(p->input, p->inbuf);
        
        if(where >= 0){
            p->input[where] = '\0';
            strncat(p->name, p->input, MAXNAME - 1);
            p->inbuf -= where + 1;
            if(p->inbuf > 0){
                memmove(p->input, &((p->input)[where + 1]), p->inbuf);
            } else {
                memset(p->input, '\0', sizeof(p->input));
                p->inbuf = 0;
            }
            p->room = sizeof(p->input) - (p->inbuf);
            p->after = &((p->input)[p->inbuf]);
            
            if(write(p->fd, confirmation, strlen(confirmation)) == -1){
                perror("write fail");
                removeclient(p->fd);
            }
            return NULL;
        }
        
        p->room = sizeof(p->input) - (p->inbuf);
        p->after = &((p->input)[p->inbuf]);
        return NULL;
    }
}

//process the tokenized user args and execute poll commands
int process_args(int cmd_argc, char **cmd_argv, Poll **poll_list_ptr, struct client *p) {
    
    // some commands need the poll_list head and others
    // require the address of that head
    Poll *poll_list = *poll_list_ptr;
    if (cmd_argc <= 0) {
        return 0;
    } else if (strcmp(cmd_argv[0], "quit") == 0 && cmd_argc == 1) {
        removeclient(p->fd);
        
    } else if (strcmp(cmd_argv[0], "list_polls") == 0 && cmd_argc == 1) {
        char *buf = print_polls(poll_list);
        //printf("%s", buf);
        if(write(p->fd, buf, strlen(buf)) ==-1){
            perror("write fail");
            removeclient(p->fd);
        }
        free(buf);
        
    } else if (strcmp(cmd_argv[0], "create_poll") == 0 && cmd_argc >= 3) {
        int label_count = cmd_argc - 2;
        int result = create_poll(cmd_argv[1], &cmd_argv[2], label_count,
        poll_list_ptr);
        if (result == 1) {
            if(write(p->fd, "Poll by this name already exists\n", strlen("Poll by this name already exists\n")) == -1){
                perror("write fail");
                removeclient(p->fd);
            }
        }
        
    } else if (strcmp(cmd_argv[0], "vote") == 0 && cmd_argc == 3) {
        char *participant_name = p->name; // name for clarity of code below
        char *poll_name = cmd_argv[1];        // better name for clarity of code below
        
        // try to add participant to this poll
        int return_code = add_participant(participant_name, poll_name, poll_list, cmd_argv[2]);
        if (return_code == 1) {
            if(write(p->fd, "Poll by this name does not exist.\n", strlen("Poll by this name does not exist.\n")) ==-1){
                perror("write fail");
                removeclient(p->fd);
            }
        } else if (return_code == 2) {
            // this poll already has this client participating so don't add
            // instead just update the vote
            return_code = update_availability(participant_name, poll_name, cmd_argv[2], poll_list);
        }
        // this could apply in either case
        if (return_code == 3) {
            if(write(p->fd, "Availability string is wrong size for this poll.\n", strlen("Availability string is wrong size for this poll.\n")) == -1){
                perror("write fail");
                removeclient(p->fd);
            }
        }
        if(return_code == 0){
            char buf[strlen(announcement) + MAXNAME];
            sprintf(buf, "There has been new activity on poll %s\n", poll_name);
            broadcast(buf, strlen(buf), find_poll(poll_name, poll_list));
        }
        
    } else if (strcmp(cmd_argv[0], "comment") == 0 && cmd_argc >= 3) {
        // first determine how long a string we need
        int space_needed = 0;
        int i;
        for (i=3; i<cmd_argc; i++) {
            space_needed += strlen(cmd_argv[i]) + 1;
        }
        // allocate the space
        char *comment = malloc(space_needed);
        if (comment == NULL) {
            perror("malloc");
            exit(1);
        }
        // copy in the bits to make a single string
        strcpy(comment, cmd_argv[2]);
        for (i=3; i<cmd_argc; i++) {
            strcat(comment, " ");
            strcat(comment, cmd_argv[i]);
        }
        
        int return_code = add_comment(p->name, cmd_argv[1], comment,
        poll_list);
        // comment was only used as parameter so we are finished with it now
        free(comment);
        if (return_code == 1) {
            if(write(p->fd, "There is no poll with this name.\n", strlen("There is no poll with this name.\n")) == -1){
                perror("write fail");
                removeclient(p->fd);
            }
        } else if (return_code == 2) {
            if(write(p->fd, "You can't comment on a poll until you vote on it\n", strlen("You can't comment on a poll until you vote on it\n")) == -1){
                perror("write fail");
                removeclient(p->fd);
            }
        }
        
    } else if (strcmp(cmd_argv[0], "delete_poll") == 0 && cmd_argc == 2) {
        if (delete_poll(cmd_argv[1], poll_list_ptr) == 1) {
            if(write(p->fd, "No poll by this name exists.\n", strlen("No poll by this name exists.\n")) == -1){
                perror("write fail");
                removeclient(p->fd);
            }
        }
        
    } else if (strcmp(cmd_argv[0], "poll_info") == 0 && cmd_argc == 2) {
        char *buf = print_poll_info(cmd_argv[1], poll_list);
        if(buf == NULL){
            if(write(p->fd, "No poll by this name exists\n", strlen("No poll by this name exists\n")) == -1){
                perror("write fail");
                removeclient(p->fd);
            }
        }
        if(buf != NULL){
            if(write(p->fd, buf, strlen(buf)) == -1){
                perror("write fail");
                removeclient(p->fd);
            }
            free(buf);
        }
    }
    else {
        if(write(p->fd, "Incorrect syntax\n", strlen("Incorrect syntax\n")) == -1){
            perror("write fail");
            removeclient(p->fd);
        }
    }
    return 0;
}

//tokenize user input to process later for poll commands
int execute_poll_commands(char *input, struct client *p){
    char *cmd_argv[INPUT_ARG_MAX_NUM];
    int cmd_argc;
    
    char *next_token = strtok(input, DELIM);
    cmd_argc = 0;
    
    while(next_token != NULL){
        cmd_argv[cmd_argc] = next_token;
        cmd_argc ++;
        next_token = strtok(NULL, DELIM);
    }
    
    process_args(cmd_argc, cmd_argv, &poll_list, p);
    free(input);
    return 0;
}

int num_clients(){
    struct client *p = top;
    int count = 0;
    while(p != NULL){
        count += 1;
        p = p->next;
    }
    return count;
}