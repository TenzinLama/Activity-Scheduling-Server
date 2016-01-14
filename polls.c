#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lists.h"

#define INPUT_BUFFER_SIZE 256
#define INPUT_ARG_MAX_NUM 12
#define DELIM " \n"


/* 
 * Print a formatted error message to stderr.
 */
void error(char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
}

/* 
 * Read and process poll commands
 * Return:  -1 for quit command
 *          0 otherwise
 */
int process_args(int cmd_argc, char **cmd_argv, Poll **poll_list_ptr) {

    // some commands need the poll_list head and others
    // require the address of that head
    Poll *poll_list = *poll_list_ptr; 

    if (cmd_argc <= 0) {
        return 0;
    } else if (strcmp(cmd_argv[0], "quit") == 0 && cmd_argc == 1) {
        return -1;
        
    } else if (strcmp(cmd_argv[0], "list_polls") == 0 && cmd_argc == 1) {
        char *buf = print_polls(poll_list);
        printf("%s", buf);
        free(buf);
        
    } else if (strcmp(cmd_argv[0], "create_poll") == 0 && cmd_argc >= 3) {
        int label_count = cmd_argc - 2;
        int result = create_poll(cmd_argv[1], &cmd_argv[2], label_count,
                     poll_list_ptr);
        if (result == 1) {
            error("Poll by this name already exists");
        }

   } else if (strcmp(cmd_argv[0], "vote") == 0 && cmd_argc == 4) {
        char *participant_name = cmd_argv[1]; // name for clarity of code below
        char *poll_name = cmd_argv[2];        // better name for clarity of code below

        // try to add participant to this poll  
        int return_code = add_participant(participant_name, poll_name, poll_list, cmd_argv[3]);
        if (return_code == 1) {
            error("Poll by this name does not exist.");
        } else if (return_code == 2) {
            // this poll already has this client participating so don't add
            // instead just update the vote
            return_code = update_availability(participant_name, poll_name, cmd_argv[3], poll_list); 
        }
        // this could apply in either case
        if (return_code == 3) {
            error("Availability string is wrong size for this poll.");
        }

    } else if (strcmp(cmd_argv[0], "add_participant") == 0 && cmd_argc == 4) {
        int return_code = add_participant(cmd_argv[1], cmd_argv[2], poll_list, 
                                          cmd_argv[3]);
        if (return_code == 1) {
           error("Poll by this name does not exist.");
        } else if (return_code == 2) {
           error("This Poll already has a participant by this name.");
        } else if (return_code == 3) {
           error("Availability string is wrong size for this poll.");
        }
      
    } else if (strcmp(cmd_argv[0], "add_comment") == 0 && cmd_argc >= 4) {
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
        strcpy(comment, cmd_argv[3]);
        for (i=4; i<cmd_argc; i++) {
            strcat(comment, " ");
            strcat(comment, cmd_argv[i]);
        }
        
        int return_code = add_comment(cmd_argv[1], cmd_argv[2], comment, 
                                      poll_list);
        // comment was only used as parameter so we are finished with it now
        free(comment);

        if (return_code == 1) {
           error("There is no poll with this name.");
        } else if (return_code == 2) {
           error("There is no participant by this name in this poll.");
        }

    } else if (strcmp(cmd_argv[0], "delete_poll") == 0 && cmd_argc == 2) {
        if (delete_poll(cmd_argv[1], poll_list_ptr) == 1) {
            error("No poll by this name exists.");
        }

    } else if (strcmp(cmd_argv[0], "poll_info") == 0 && cmd_argc == 2) {
        char *buf = print_poll_info(cmd_argv[1], poll_list);
        if(buf == NULL){
            printf("No poll by this name exists\n");
        }
        if(buf != NULL){
            printf("%s", buf);
            free(buf);
        }     
    }
     else {
        error("Incorrect syntax");
    }
    return 0;
}


int main(int argc, char* argv[]) {
    int batch_mode = (argc == 2);
    char input[INPUT_BUFFER_SIZE];
    FILE *input_stream;

    // for holding arguments to individual commands passed to sub-procedure
    char *cmd_argv[INPUT_ARG_MAX_NUM];
    int cmd_argc;

    // Create the heads of the empty data structure
    Poll *poll_list= NULL;

    if (batch_mode) {
        input_stream = fopen(argv[1], "r");
        if (input_stream == NULL) {
            perror("Error opening file");
            exit(1);
        }
    } else {
        // interactive mode 
        input_stream = stdin;
    }

    printf("Welcome to the Simple Scheduling Tool!\nPlease type a command:\n>");
    
    while (fgets(input, INPUT_BUFFER_SIZE, input_stream) != NULL) {
        // only echo the line in batch mode since in interactive mode the user
        // just typed the line
        if (batch_mode) {
            printf("%s", input);
        }
        // tokenize arguments
        // Notice that this tokenizing is not sophisticated enough to handle 
        // quoted arguments with spaces so poll names, participant names and
        // and slot names can not have spaces. Comments can have multiple words

        char *next_token = strtok(input, DELIM);
        cmd_argc = 0;
        while (next_token != NULL) {
            if (cmd_argc >= INPUT_ARG_MAX_NUM - 1) {
                error("Too many arguments!");
                cmd_argc = 0;
                break;
            }
            cmd_argv[cmd_argc] = next_token;
            cmd_argc++;
            next_token = strtok(NULL, DELIM);
        }
        if (cmd_argc > 0 && process_args(cmd_argc, cmd_argv, &poll_list) == -1) {
            break; // can only reach if quit command was entered
        }
        printf(">");
    }

    if (batch_mode) {
        fclose(input_stream);
    }
    return 0;
 }
