#ifndef SHELL_IO_H
#define SHELL_IO_H

#include <sys/types.h>
#include <stddef.h>

enum { SHELL_MAX_LINE = 512 };
#define SHELL_DELIMS " \t\r\n"

void sh_print(const char *s);
void sh_error(const char *prefix, const char *msg);

/* Reads one line from stdin into buf (NUL-terminated).
   Returns: bytes read (>0), 0 on EOF, -1 on error, -2 if line too long (drains rest). */
ssize_t sh_readline(char *buf, size_t buf_sz);

/* Tokenizes `line` in-place and allocates duplicates into `tokens`.
   tokens is terminated with NULL. Returns number of tokens. */
size_t sh_tokenize(char *line, char **tokens);

#endif