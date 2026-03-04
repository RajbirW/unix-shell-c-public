#include "shell_io.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static size_t cstrnlen(const char *s, size_t max) {
    size_t n = 0;
    while (n < max && s && s[n] != '\0') n++;
    return n;
}

void sh_print(const char *s) {
    if (!s) return;
    write(STDOUT_FILENO, s, cstrnlen(s, 1 << 20));
}

void sh_error(const char *prefix, const char *msg) {
    if (prefix) write(STDERR_FILENO, prefix, cstrnlen(prefix, 1 << 20));
    if (msg)    write(STDERR_FILENO, msg,    cstrnlen(msg,    1 << 20));
    write(STDERR_FILENO, "\n", 1);
}

ssize_t sh_readline(char *buf, size_t buf_sz) {
    if (!buf || buf_sz < 2) return -1;

    ssize_t n = read(STDIN_FILENO, buf, buf_sz - 1);
    if (n <= 0) { // 0 = EOF, -1 = error
        if (n == 0) buf[0] = '\0';
        return n;
    }

    buf[n] = '\0';

    // If we didn't read a newline and buffer filled, the line may be too long.
    if ((size_t)n == buf_sz - 1 && strchr(buf, '\n') == NULL) {
        // Drain until newline or EOF
        int ch;
        while ((ch = getchar()) != EOF && ch != '\n') {}
        buf[0] = '\0';
        return -2;
    }

    return n;
}

size_t sh_tokenize(char *line, char **tokens) {
    if (!line || !tokens) return 0;

    size_t count = 0;
    char *tok = strtok(line, SHELL_DELIMS);
    while (tok) {
        tokens[count] = strdup(tok);
        if (!tokens[count]) {
            fprintf(stderr, "ERROR: out of memory\n");
            exit(1);
        }
        count++;
        tok = strtok(NULL, SHELL_DELIMS);
    }
    tokens[count] = NULL;
    return count;
}