#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <stdlib.h>
#include <stdio.h> 
#include <signal.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "builtins.h"
#include "shell_io.h"
#include "commands.h"


struct server_info *server_list = NULL;
int is_client_active = 0;
// ====== Command execution =====

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd) {
    ssize_t cmd_num = 0;
    while (cmd_num < BUILTINS_COUNT && strcmp(BUILTINS[cmd_num], cmd) != 0) {
        cmd_num += 1;
    }
    return BUILTINS_FN[cmd_num];
}



// ===== Builtins =====

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error ... but there are no errors on echo. 
 */
ssize_t bn_echo(char **tokens) {
    ssize_t index = 1;

    char the_output[SHELL_MAX_LINE];
    the_output[0] = '\0';

    // build output safely
    while (tokens[index] != NULL) {
        if (strlen(the_output) + strlen(tokens[index]) + 2 >= sizeof(the_output)) {
            break;
        }
        if (index > 1) strncat(the_output, " ", sizeof(the_output) - strlen(the_output) - 1);
        strncat(the_output, tokens[index], sizeof(the_output) - strlen(the_output) - 1);
        index++;
    }

    strncat(the_output, "\n", sizeof(the_output) - strlen(the_output) - 1);
    sh_print(the_output);
    return 0;
}

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error
 */

 ssize_t bn_ls(char **tokens) {
    int recursive = 0;
    int depth = -1;
    char *filter = NULL;
    char *path = ".";  // Default to current directory

    // Iterate over arguments
    for (int i = 1; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "--rec") == 0) {
            recursive = 1;
        } else if (strcmp(tokens[i], "--d") == 0) {
            if (tokens[i + 1] == NULL || !isdigit(tokens[i + 1][0])) {
                sh_error("ERROR: --d must be followed by a valid depth value", "");
                return -1;
            }
            depth = atoi(tokens[i + 1]);
            i++;  // Skip the next token (which is the depth value)
        } else if (strcmp(tokens[i], "--f") == 0) {
            if (tokens[i + 1] == NULL) {
                sh_error("ERROR: --f must be followed by a substring", "");
                return -1;
            }
            filter = tokens[i + 1];
            i++;  // Skip the next token (which is the substring)
        } else if (tokens[i][0] != '-') {
            path = tokens[i];  // Assume it's the path argument
        } else {
            sh_error("ERROR: Unknown flag", tokens[i]);
            return -1;
        }
    }

    // Ensure --d is not used without --rec
    if (depth != -1 && !recursive) {
        sh_error("ERROR: --d requires --rec", "");
        return -1;
    }

    return list_files(path, 0, depth, filter, recursive);
}

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error
 */

ssize_t bn_cd(char **tokens){
    if(tokens[1] == NULL){
        // No path provided, default to home directory.
        char *home = getenv("HOME");
        if (home == NULL){
            sh_error("ERROR: HOME directory not set", "");
            return -1;
        }
        if(chdir(home) != 0){
            sh_error("ERROR: Invalid path", home);
            return -1;
        }
        return 0;
    }

    char *path = tokens[1];
    if(strcmp(path, ".") == 0) {
        return 0; // Stay in current directory
    }
    else if(strcmp(path, "..") == 0) {
        // Change to parent directory
        if(chdir("..") != 0){
            sh_error("ERROR: Could not switch directories", path);
            return -1;
        }
        return 0;
    }
    else if(strcmp(path, "...") == 0 || strcmp(path, "../..") == 0) {
        // Change 2 directories up
        if(chdir("../..") != 0){
            sh_error("ERROR: Could not switch directories", path);
            return -1;
        }
        return 0;
    }
    else if(strcmp(path, "....") == 0 || strcmp(path, "../../..") == 0) {
        // Change 3 directories up
        if(chdir("../../..") != 0){
            sh_error("ERROR: Could not switch directories", path);
            return -1;
        }
        return 0;
    }
    else {
        if(chdir(path) != 0){
            sh_error("ERROR: Invalid path", path);
            return -1;
        }
        return 0;
    } 
}

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error
 */
ssize_t bn_cat(char **tokens) {
    if (tokens[1] == NULL) {
        // No file provided: Read from standard input
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            sh_print(buffer);
        }
        return 0;
    }

    // Read from file
    char *fileName = tokens[1];
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        sh_error("ERROR: Cannot open file", fileName);
        return -1;
    }

    char buffer[1024]; 
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        sh_print(buffer);
    }

    fclose(file);
    return 0;
}

ssize_t bn_wc(char **tokens) {
    FILE *file = NULL;
    int from_stdin = 0;

    if (tokens[1] == NULL) {
        file = stdin;  // Read from stdin if no file is given
        from_stdin = 1;
    } else {
        file = fopen(tokens[1], "r");
        if (file == NULL) {
            sh_error("ERROR: Cannot open file", tokens[1]);
            return -1;
        }
    }

    ssize_t word_count = 0, char_count = 0, newline_count = 0;
    int in_word = 0;
    char ch;

    while ((ch = fgetc(file)) != EOF) {
        char_count++;

        if (ch == '\n') {
            newline_count++;
        }

        if (ch == ' ' || ch == '\t' || ch == '\n') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            word_count++;
        }
    }

    if (!from_stdin) {
        fclose(file);
    }

    // Ensure output is formatted correctly
    char output[256];
    snprintf(output, sizeof(output),
             "word count %zd\ncharacter count %zd\nnewline count %zd\n",
             word_count, char_count, newline_count);
    sh_print(output);

    return 0;
}



/* 
 * Return 0 on success and -1 on error
 */
ssize_t list_files(char *path, int depth, int max_depth, char *substring, int recursive) {
    depth++;  // Increment depth when entering a new directory
    if (max_depth != -1 && depth > max_depth) {
        return 0;
    }

    DIR *dir = opendir(path);
    if (dir == NULL) {
        sh_error("ERROR: Invalid path: ", path);
        return -1;
    }

    struct dirent *entry;
    char new_path[1024];

    // Display "." and ".." when listing a directory
    if(substring == NULL){
        sh_print(".\n");
        sh_print("..\n");
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // We already printed these manually
        }

        // Print matching files and directories
        if (substring == NULL || strstr(entry->d_name, substring) != NULL) {
            sh_print(entry->d_name);
            sh_print("\n");
        }

        // Check if it's a directory before recursion
        struct stat path_stat;
        int len = snprintf(new_path, sizeof(new_path), "%s/%s", path, entry->d_name);

        if ((size_t) len >= sizeof(new_path)) {
            sh_error("ERROR: Path too long", "");
            closedir(dir);
            return -1;
        }

        if (stat(new_path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
            if (recursive && (max_depth == -1 || depth < max_depth)) {
                list_files(new_path, depth, max_depth, substring, recursive);
            }
        }
    }

    closedir(dir);
    return 0;
}

ssize_t bn_kill(char **tokens) {
    // Check if the command has the correct number of arguments
    if (tokens[1] == NULL) {
        sh_error("ERROR: Missing PID", "");
        return -1;
    }

    // Parse the PID
    pid_t pid = atoi(tokens[1]);
    if (pid <= 0) {
        sh_error("ERROR: Invalid PID: ", tokens[1]);
        return -1;
    }

    // Parse the signal number (if provided)
    int signum = SIGTERM; // Default signal is SIGTERM
    if (tokens[2] != NULL) {
        signum = atoi(tokens[2]);
        if (signum <= 0) {
            sh_error("ERROR: Invalid signal specified: ", tokens[2]);
            return -1;
        }
    }

    // Send the signal to the process
    if (kill(pid, signum) == -1) {
        if (errno == ESRCH) {
            sh_error("ERROR: The process does not exist", "");
        } else if (errno == EINVAL) {
            sh_error("ERROR: Invalid signal specified", "");
        } else {
            sh_error("ERROR: Failed to send signal", "");
        }
        return -1;
    }

    return 0; // Success
}
ssize_t bn_ps(char **tokens) {
    (void)tokens; // Unused parameter

    // Iterate through the job list and print process names and IDs
    struct job *curr = job_list;
    while (curr != NULL) {
        char output[SHELL_MAX_LINE];
        snprintf(output, sizeof(output), "%s %d\n", curr->command, curr->pid);
        sh_print(output);
        curr = curr->next;
    }

    return 0; // Success
}

ssize_t bn_startServer(char **tokens) {
    if (tokens[1] == NULL) {
        sh_error("ERROR: No port provided", "");
        return -1;
    }

    int port = atoi(tokens[1]);
    if (port <= 0 || port > 65535) {
        sh_error("ERROR: Invalid port number", tokens[1]);
        return -1;
    }

    // Check for existing server
    struct server_info *curr = server_list;
    while (curr != NULL) {
        if (curr->port == port) {
            sh_error("ERROR: Server already running on port", tokens[1]);
            return -1;
        }
        curr = curr->next;
    }

    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        sh_error("ERROR: Socket creation failed", "");
        return -1;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        sh_error("ERROR: setsockopt failed", "");
        close(server_fd);
        return -1;
    }

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        sh_error("ERROR: Binding failed", "");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) < 0) {
        sh_error("ERROR: Listening failed", "");
        close(server_fd);
        return -1;
    }

    // Fork server process
    pid_t pid = fork();
    if (pid == 0) {
        setsid();  // Make child process leader
        // Child process - the actual server
        
        signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe signals
        
        struct client_info *clients = NULL;
        int client_counter = 0;
        fd_set read_fds;
        int max_fd;
        struct timeval timeout;
        
        while (1) {
            FD_ZERO(&read_fds);
            FD_SET(server_fd, &read_fds);
            max_fd = server_fd;

            // Add all clients to fd_set
            struct client_info *client = clients;
            while (client != NULL) {
                FD_SET(client->fd, &read_fds);
                if (client->fd > max_fd) max_fd = client->fd;
                client = client->next;
            }

            // Wait for activity with timeout
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
            
            if (activity < 0 && errno != EINTR) {
                continue;
            }

            // Handle new connections
            if (FD_ISSET(server_fd, &read_fds)) {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
                
                if (client_fd >= 0) {
                    // Assign client ID
                    client_counter++;
                    struct client_info *new_client = malloc(sizeof(struct client_info));
                    if (!new_client) {
                        close(client_fd);
                        continue;
                    }

                    new_client->fd = client_fd;
                    new_client->id = client_counter;
                    snprintf(new_client->id_str, sizeof(new_client->id_str), "client%d:", client_counter);
                    new_client->next = clients;
                    clients = new_client;

                    // Send ID to client
                    if (send(client_fd, new_client->id_str, strlen(new_client->id_str), 0) < 0) {
                        close(client_fd);
                        free(new_client);
                        continue;
                    }
                }
            }

            // Handle client messages
            struct client_info *prev = NULL;
            client = clients;
            while (client != NULL) {
                if (FD_ISSET(client->fd, &read_fds)) {
                    char buffer[1024];
                    ssize_t valread = recv(client->fd, buffer, sizeof(buffer) - 1, 0); // Non-blocking read

                    if (valread <= 0) {
                        // Client disconnected
                        close(client->fd);
                        if (prev == NULL) {
                            clients = client->next;
                        } else {
                            prev->next = client->next;
                        }
                        struct client_info *to_free = client;
                        client = client->next;
                        free(to_free);
                        continue;
                    } else {
                        buffer[valread] = '\0';

                        // Handle special commands
                        if (strcmp(buffer, "\\connected") == 0) {
                            int count = 0;
                            for (struct client_info *tmp = clients; tmp; tmp = tmp->next) count++;
                            
                            // Format response with client ID and newline
                            char response[128];
                            snprintf(response, sizeof(response), "%s %d clients connected\n", 
                                     client->id_str, count);
                            
                            // Send response but DON'T close connection
                            if (send(client->fd, response, strlen(response), 0) < 0) {
                                // Only close if send fails
                                close(client->fd);
                                // Remove client from list
                                if (prev == NULL) {
                                    clients = client->next;
                                } else {
                                    prev->next = client->next;
                                }
                                free(client);
                            }
                            continue;  // Skip normal message broadcasting
                        }

                        // Format message with client ID
                        char formatted_msg[1100];
                        snprintf(formatted_msg, sizeof(formatted_msg), "%s %s", client->id_str, buffer);
                        
                        // Display on server console
                        sh_print(formatted_msg);
                        sh_print("\n");

                        // Send to all other clients
                        struct client_info *recipient = clients;
                        while (recipient != NULL) {
                            if (recipient != client) {
                                if (send(recipient->fd, formatted_msg, strlen(formatted_msg), 0) < 0) {
                                    // Mark for disconnection
                                    close(recipient->fd);
                                }
                            }
                            recipient = recipient->next;
                        }
                        // Send special echo version back to sender
                        
                        // snprintf(echo_msg, sizeof(echo_msg), "You: %s", buffer);  // Different format for sender
                        send(client->fd, formatted_msg, strlen(formatted_msg), 0);
                        }
                }
                prev = client;
                client = client->next;
            }
        }
    } else if (pid > 0) {
        // Parent process - register server
        struct server_info *new_server = malloc(sizeof(struct server_info));
        if (!new_server) {
            sh_error("ERROR: Memory allocation failed", "");
            close(server_fd);
            return -1;
        }

        *new_server = (struct server_info){
            .port = port,
            .pid = pid,
            .socket_fd = server_fd,
            .clients = NULL,
            .next = server_list
        };
        server_list = new_server;
    } else {
        sh_error("ERROR: Fork failed", "");
        close(server_fd);
        return -1;
    }
    return 0;
}

ssize_t bn_closeServer(char **tokens) {
    (void)tokens; // NOT USED
    // No port argument needed since only one server exists
    if (server_list == NULL) {
        sh_error("ERROR: No server is currently running", "");
        return -1;
    }

    // There should only be one server in the list
    struct server_info *server_to_close = server_list;
    
    // Terminate the server process
    if (server_to_close->pid > 0) {
        if (kill(-server_to_close->pid, SIGTERM) < 0) {
            sh_error("ERROR: Failed to terminate server process", "");
            return -1;
        }
    }

    // Close server socket
    if (server_to_close->socket_fd >= 0) {
        close(server_to_close->socket_fd);
    }

    // Clean up any connected clients
    struct client_info *client = server_to_close->clients;
    while (client != NULL) {
        struct client_info *next = client->next;
        if (client->fd >= 0) {
            close(client->fd);
        }
        free(client);
        client = next;
    }

    // Remove from server list
    server_list = server_to_close->next;
    free(server_to_close);

    sh_print("Server closed successfully\n");
    return 0;
}

ssize_t bn_send(char **tokens) {
    if (tokens[1] == NULL || tokens[2] == NULL) {
        sh_error("ERROR: Usage: send port-number hostname message", "");
        return -1;
    }

    int port = atoi(tokens[1]);
    if (port <= 0 || port > 65535) {
        sh_error("ERROR: Invalid port number", tokens[1]);
        return -1;
    }

    char *hostname = tokens[2];

    // Combine all remaining tokens into the message
    char message[SHELL_MAX_LINE] = {0};
    for (int i = 3; tokens[i] != NULL; i++) {
        if (i > 3) {
            strncat(message, " ", sizeof(message) - strlen(message) - 1);
        }
        strncat(message, tokens[i], sizeof(message) - strlen(message) - 1);
        
        // Check if we're running out of space
        if (strlen(message) >= sizeof(message) - 1) {
            sh_error("ERROR: Message too long", "");
            return -1;
        }
    }

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        sh_error("ERROR: Socket creation failed", "");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert hostname to IP address
    if (inet_pton(AF_INET, hostname, &serv_addr.sin_addr) <= 0) {
        sh_error("ERROR: Invalid address / Address not supported", "");
        close(sock);
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        sh_error("ERROR: Connection failed", "");
        close(sock);
        return -1;
    }

    // Send message (including null terminator)
    size_t message_len = strlen(message);
    ssize_t sent = send(sock, message, message_len, 0);
    if (sent < 0 || (size_t)sent != message_len) {
        sh_error("ERROR: Message send failed", "");
        close(sock);
        return -1;
    }

    // Display what we sent
    sh_print(message);
    sh_print("\n");

    close(sock);
    return 0;
}

ssize_t bn_startClient(char **tokens) {
    if (tokens[1] == NULL) {
        sh_error("ERROR: No port provided", "");
        is_client_active = 0;
        should_exit_client = 0;
        return -1;
    }
    if (tokens[2] == NULL) {
        sh_error("ERROR: No hostname provided", "");
        is_client_active = 0;
        should_exit_client = 0;
        return -1;
    }

    int port = atoi(tokens[1]);
    if (port <= 0 || port > 65535) {
        sh_error("ERROR: Invalid port number", tokens[1]);
        is_client_active = 0;
        should_exit_client = 0;
        return -1;
    }

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        sh_error("ERROR: Socket creation failed", "");
        is_client_active = 0;
        should_exit_client = 0;
        return -1;
    }

    // Ignore broken pipe signals
    signal(SIGPIPE, SIG_IGN);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert hostname to IP address
    if (inet_pton(AF_INET, tokens[2], &serv_addr.sin_addr) <= 0) {
        sh_error("ERROR: Invalid address / Address not supported", tokens[2]);
        close(sock);
        is_client_active = 0;
        should_exit_client = 0;
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
        sh_error("ERROR: Connection failed", "");
        close(sock);
        is_client_active = 0;
        should_exit_client = 0;
        return -1;
    }

    // Get client ID from server
    char id_buffer[32] = {0};
    ssize_t id_len = recv(sock, id_buffer, sizeof(id_buffer) - 1, 0);
    if (id_len <= 0) {
        sh_error("ERROR: Failed to get client ID", "");
        close(sock);
        is_client_active = 0;
        should_exit_client = 0;
        return -1;
    }
    id_buffer[id_len] = '\0';

    is_client_active = 1;

    // Main message loop
    fd_set read_fds;
    int max_fd;
    char input[SHELL_MAX_LINE];
    int prompt_shown = 0;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        max_fd = sock > STDIN_FILENO ? sock : STDIN_FILENO;

        // Only show prompt if we haven't already shown one
        if (!prompt_shown) {
            sh_print("> ");
            fflush(stdout);
            prompt_shown = 1;
        }

        struct timeval timeout = {0, 100000};
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if(should_exit_client) {
            shutdown(sock, SHUT_RDWR);  // Gracefully close socket
            close(sock);
            is_client_active = 0;
            should_exit_client = 0;

            clearerr(stdin);  // Clear EOF/error flags
            fflush(stdin);    // Flush any residual data (optional but safe)
            
            sh_print("Disconnected\n");
            return 0;  // CTRL+C pressed
            break;
        }

        if (activity < 0 && errno != EINTR) {
            continue;
        }

        // Handle server messages first
        if (FD_ISSET(sock, &read_fds)) {
            char buffer[1024];
            ssize_t valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
            
            if (valread <= 0) {
                sh_print("\nServer disconnected\n");
                break;
            }
            if(should_exit_client) {
                break;
            }
            
            buffer[valread] = '\0';
            // Clear the current line before showing server message
            sh_print("\r");  // Return to start of line
            sh_print(buffer);
            if (buffer[strlen(buffer)-1] != '\n') {
                sh_print("\n");
            }
            prompt_shown = 0;  // Reset prompt flag after message
        }

        // Then handle user input
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(input, sizeof(input), stdin) == NULL) {
                shutdown(sock, SHUT_RDWR);  // Gracefully close socket
                close(sock);
                is_client_active = 0;
                should_exit_client = 0;

                clearerr(stdin);  // Clear EOF/error flags
                fflush(stdin);    // Flush any residual data (optional but safe)

                sh_print("Disconnected\n");
                return 0;  // CTRL+D pressed
            }
            
            input[strcspn(input, "\n")] = '\0';
            
            if (send(sock, input, strlen(input), 0) < 0) {
                sh_error("ERROR: Failed to send message", "");
                is_client_active = 0;
                should_exit_client = 0;
                break;
            }
            prompt_shown = 0;  // Reset prompt flag after sending
        }
    }

    close(sock);
    sh_print("Disconnected\n");

    is_client_active = 0;
    should_exit_client = 0;
    return 0;
}

void cleanup_servers() {
    struct server_info *curr = server_list;
    while (curr != NULL) {
        struct server_info *temp = curr;
        curr = curr->next;
        
        // Kill the server process group
        if (temp->pid > 0) {
            kill(-temp->pid, SIGTERM);  // Negative PID kills entire process group
            waitpid(temp->pid, NULL, 0);  // Wait for the process to terminate
        }
        
        // Close server socket
        if (temp->socket_fd >= 0) {
            close(temp->socket_fd);
        }
        
        // Clean up clients
        struct client_info *client = temp->clients;
        while (client != NULL) {
            struct client_info *next = client->next;
            if (client->fd >= 0) {
                close(client->fd);
            }
            free(client);
            client = next;
        }
        
        free(temp);
    }
    server_list = NULL;
}

