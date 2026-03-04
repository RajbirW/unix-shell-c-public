#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include "builtins.h"
#include "variables.h"
#include "shell_io.h"
#include "commands.h"

// Define global job list
struct job *job_list = NULL;
int next_job_id = 1;
struct job *completed_jobs = NULL;
volatile sig_atomic_t should_exit_client = 0;

// Function to execute external commands
void execute_external_command(char **tokens) {
    if (execvp(tokens[0], tokens) == -1) {
        sh_error("ERROR: Command not found: ", tokens[0]);
        exit(1); // Exit the child process with an error code
    }
}

/* Signal handler for SIGINT */
void handler(int signum) {
    if (signum == SIGINT) {
        if (is_client_active == 1) {
            should_exit_client = 1;  // Signal client to exit
        } else {
            sh_print("\n");          // Shell mode: Just print newline
        }
    }
}

int execute_piped_command(char **left_cmd, char **right_cmd) {
    int pipefd[2];
    pid_t pid1, pid2;

    if (pipe(pipefd) == -1) {
        sh_error("ERROR: Pipe creation failed", "");
        return -1;
    }

    // First child: Executes left command
    if ((pid1 = fork()) == 0) {
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
        close(pipefd[0]);
        close(pipefd[1]);

        // Handle variable assignments locally
        if (left_cmd[0] && strchr(left_cmd[0], '=') != NULL) {
            set_variable(left_cmd[0]);
            exit(0);
        }

        // Expand variables in the left command
        for (int i = 0; left_cmd[i] != NULL; i++) {
            if (left_cmd[i][0] == '$' && strlen(left_cmd[i]) > 1) {
                char *var_value = get_variable(left_cmd[i] + 1);
                if (var_value != NULL) {
                    free(left_cmd[i]);
                    left_cmd[i] = var_value;
                }
            }
        }

        // Execute the left command
        bn_ptr builtin_fn = check_builtin(left_cmd[0]);
        if (builtin_fn) {
            builtin_fn(left_cmd);
            exit(0);
        } else {
            execvp(left_cmd[0], left_cmd);
            sh_error("ERROR: Unknown command: ", left_cmd[0] ? left_cmd[0] : "");
            exit(1);
        }
    }

    // Second child: Executes right command
    if ((pid2 = fork()) == 0) {
        dup2(pipefd[0], STDIN_FILENO); // Redirect stdin to pipe
        close(pipefd[1]);
        close(pipefd[0]);

        // Expand variables in the right command
        for (int i = 0; right_cmd[i] != NULL; i++) {
            if (right_cmd[i][0] == '$' && strlen(right_cmd[i]) > 1) {
                char *var_value = get_variable(right_cmd[i] + 1);
                if (var_value != NULL) {
                    free(right_cmd[i]);
                    right_cmd[i] = var_value;
                }
            }
        }

        // Execute the right command
        bn_ptr builtin_fn = check_builtin(right_cmd[0]);
        if (builtin_fn) {
            builtin_fn(right_cmd);
            exit(0);
        } else {
            execvp(right_cmd[0], right_cmd);
            sh_error("ERROR: Unknown command: ", right_cmd[0] ? right_cmd[0] : "");
            exit(1);
        }
    }

    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;
}

// Function to check if a command is a background command
int is_background_command(char **tokens) {
    int i = 0;
    while (tokens[i] != NULL) {
        i++;
    }
    if (i > 0 && strcmp(tokens[i - 1], "&") == 0) {
        free(tokens[i - 1]);
        tokens[i - 1] = NULL;
        return 1;
    }
    return 0;
}

// Function to add a job to the list
void add_job(pid_t pid, char **tokens) {
    struct job *new_job = malloc(sizeof(struct job));
    if (new_job == NULL) {
        perror("malloc");
        exit(1);
    }

    new_job->job_id = next_job_id;
    new_job->pid = pid;

    // Concatenate the tokens to form the full command string
    char full_command[SHELL_MAX_LINE];
    full_command[0] = '\0';

    for (int i = 0; tokens[i] != NULL; i++) {
        strncat(full_command, tokens[i], sizeof(full_command) - strlen(full_command) - 1);
        if (tokens[i + 1] != NULL) {
            strncat(full_command, " ", sizeof(full_command) - strlen(full_command) - 1);
        }
    }

    new_job->command = strdup(full_command);
    if (new_job->command == NULL) {
        perror("strdup");
        free(new_job);
        exit(1);
    }

    new_job->next = job_list;
    job_list = new_job;

    char output[SHELL_MAX_LINE];
    snprintf(output, sizeof(output), "[%d] %d\n", new_job->job_id, new_job->pid);
    sh_print(output);

    next_job_id++;
}

void print_completed_jobs(void) {
    while (completed_jobs != NULL) {
        char output[SHELL_MAX_LINE];
        snprintf(output, sizeof(output), "[%d]+  Done %s\n",
                 completed_jobs->job_id,
                 completed_jobs->command ? completed_jobs->command : "");
        sh_print(output);

        struct job *temp = completed_jobs;
        completed_jobs = completed_jobs->next;
        free(temp->command);
        free(temp);
    }
}