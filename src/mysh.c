#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>  // Needed for file redirection
#include <termios.h>
#include <netinet/in.h>  // for struct sockaddr_in
#include <sys/socket.h>

#include "builtins.h"
#include "variables.h"
#include "shell_io.h"
#include "commands.h"


void free_job_list() {
    struct job *curr = job_list;
    while (curr != NULL) {
        struct job *temp = curr;
        curr = curr->next;
        free(temp->command);
        free(temp);
    }
    job_list = NULL;
}

void sigchld_handler(int signum) {
    (void)signum; // Unused parameter
    int status;
    pid_t pid;

    // Reap all terminated child processes
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Remove the job from the job list
        struct job *prev = NULL;
        struct job *curr = job_list;

        while (curr != NULL) {
            if (curr->pid == pid) {
                // Add the completed job to the completed_jobs list
                curr->next = completed_jobs;
                completed_jobs = curr;

                // Remove the job from the job list
                if (prev == NULL) {
                    job_list = curr->next;
                } else {
                    prev->next = curr->next;
                }
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    }
}

int main(__attribute__((unused)) int argc, 
         __attribute__((unused)) char* argv[]) {
    // Save the original terminal state
    

    char *prompt = "mysh$ "; // TODO Step 1, Uncomment this.

    char input_buf[SHELL_MAX_LINE];
    input_buf[SHELL_MAX_LINE - 1] = '\0';
    char *token_arr[SHELL_MAX_LINE] = {NULL};   

    // Set up signal handler for SIGINT
    struct sigaction newact;
    newact.sa_handler = handler;
    newact.sa_flags = 0;
    sigemptyset(&newact.sa_mask);
    sigaction(SIGINT, &newact, NULL);

    
    // Set up signal handler for SIGCHLD
    struct sigaction sigchld_act;
    sigchld_act.sa_handler = sigchld_handler;
    sigchld_act.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigemptyset(&sigchld_act.sa_mask);
    sigaction(SIGCHLD, &sigchld_act, NULL);

    while (1) {
        print_completed_jobs();
        sh_print(prompt);
        fflush(stdout);

        ssize_t ret = sh_readline(input_buf, sizeof(input_buf));
        if (ret == -2) { // line too long
            sh_error("ERROR:", "input line too long");
            continue;
        }
        size_t token_count = sh_tokenize(input_buf, token_arr);

        if (ret == 0) {
            sh_print("\n");
            free_tokens(token_arr);
            free_job_list();
            cleanup_servers();
            break;
        }
        if (token_count == 0) {
            continue;
        }
        if (ret != -1 && (strncmp("exit", token_arr[0], 5) == 0)) {
            free_tokens(token_arr);
            free_job_list();
            cleanup_servers();
            break;
        }

        // Check if the command is a background command
        int is_background = is_background_command(token_arr);
        
        pid_t pid = -5;

        // Handle background commands
        if (is_background) {
            pid = fork();
        }
    
        if((is_background && pid == 0) || !is_background){
            // Handle pipes first
            int pipe_index = -1;
            for (int i = 0; token_arr[i] != NULL; i++) {
                if (strcmp(token_arr[i], "|") == 0) {
                    pipe_index = i;
                    break;
                }
            }

            if (pipe_index != -1) {
                // Split command into left and right
                char *left_cmd[SHELL_MAX_LINE] = {NULL};
                char *right_cmd[SHELL_MAX_LINE] = {NULL};

                for (int i = 0; i < pipe_index; i++) {
                    left_cmd[i] = token_arr[i];
                }
                left_cmd[pipe_index] = NULL; // Null terminate

                for (int i = pipe_index + 1, j = 0; token_arr[i] != NULL; i++, j++) {
                    right_cmd[j] = token_arr[i];
                }
                right_cmd[token_count - pipe_index - 1] = NULL; // Null terminate

                execute_piped_command(left_cmd, right_cmd);
            } else {
                // Handle non-pipe commands
                // Check if the input is an assignment statement
                if (strchr(token_arr[0], '=') != NULL) {
                    set_variable(token_arr[0]);
                    free_tokens(token_arr);
                    continue;
                }

                // Expand variables in the command
                for (int i = 0; token_arr[i] != NULL; i++) {
                    if (token_arr[i][0] == '$' && strlen(token_arr[i]) > 1) {
                        char *var_value = get_variable(token_arr[i] + 1);
                        if (var_value != NULL) {
                            free(token_arr[i]);
                            token_arr[i] = var_value;
                        }
                    }
                }

                // Execute the command
                bn_ptr builtin_fn = check_builtin(token_arr[0]);
                if (builtin_fn != NULL) {
                    // Execute built-in command
                    ssize_t err = builtin_fn(token_arr);
                    if (err == -1) {
                        sh_error("ERROR: Builtin failed: ", token_arr[0]);
                    }
                } else {
                    // Execute external command
                    pid_t pid = fork();
                    if (pid == 0) {
                        // Child process: Execute the external command
                        execute_external_command(token_arr);
                    } else if (pid > 0) {
                        // Parent process: Wait for the child to finish
                        waitpid(pid, NULL, 0);
                    } else {
                        // Fork failed
                        sh_error("ERROR: Fork failed", "");
                    }
                }
            }
            free_tokens(token_arr);
            if(pid == 0){
                exit(0);
            }
        }
        else if (is_background && pid > 0){
            add_job(pid, token_arr);
        }
        else if(pid < 0){
            sh_error("ERROR: Fork failed", "");
        }
        free_tokens(token_arr);
        
    }
    free_job_list();
    cleanup_servers();
    return 0;
}
