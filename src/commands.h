#ifndef COMMANDS_H
#define COMMANDS_H

#include <sys/types.h>
#include <signal.h>

// Data structure for background jobs
struct job {
    int job_id;
    pid_t pid;
    char *command;
    struct job *next;
};

// Global job list
extern struct job *job_list;
extern int next_job_id;
extern struct job *completed_jobs;
extern volatile sig_atomic_t should_exit_client;

// Job control
void add_job(pid_t pid, char **tokens);
void print_completed_jobs(void);
int is_background_command(char **tokens);

// Execution
void execute_external_command(char **tokens);
int execute_piped_command(char **left_cmd, char **right_cmd);

// Signal handler for SIGINT
void handler(int signum);

#endif // COMMANDS_H