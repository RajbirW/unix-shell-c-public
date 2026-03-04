#ifndef __BUILTINS_H__
#define __BUILTINS_H__

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

struct client_info {
    int fd;                    // Client socket file descriptor
    int id;                    // Numeric client ID (1, 2, 3...)
    char id_str[32];           // String version of ID ("client1", "client2")
    struct client_info *next;   // Pointer to next client in list
};

struct server_info {
    int port;
    pid_t pid;
    int socket_fd;
    struct client_info *clients;
    struct server_info *next;
};

struct client_session {
    int sock;
    int id;
    struct sockaddr_in serv_addr;
};

extern struct server_info *server_list;
extern int is_client_active;

/* Type for builtin handling functions
 * Input: Array of tokens
 * Return: >=0 on success and -1 on error
 */
typedef ssize_t (*bn_ptr)(char **);
ssize_t bn_echo(char **tokens);
ssize_t bn_ls(char **tokens);
ssize_t bn_cd(char **tokens);
ssize_t bn_cat(char **tokens);
ssize_t bn_wc(char **tokens);
ssize_t bn_kill(char ** tokens);
ssize_t bn_ps(char **tokens);
ssize_t bn_startServer(char **tokens);
ssize_t bn_closeServer(char **tokens);
ssize_t bn_send(char **tokens);
ssize_t bn_startClient(char **tokens);

ssize_t list_files(char *path, int depth, int max_depth, char *substring, int recursive);

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd);
void cleanup_servers();

/* BUILTINS and BUILTINS_FN are parallel arrays of length BUILTINS_COUNT
 */
static const char * const BUILTINS[] = {"echo", "ls", "cd", "cat", "wc", "kill", "ps", "start-server", "close-server", "send", "start-client"};
static const bn_ptr BUILTINS_FN[] = {bn_echo, bn_ls, bn_cd, bn_cat, bn_wc, bn_kill, bn_ps, bn_startServer, bn_closeServer, bn_send, bn_startClient, NULL};    // Extra null element for 'non-builtin'
static const ssize_t BUILTINS_COUNT = sizeof(BUILTINS) / sizeof(char *);

#endif
