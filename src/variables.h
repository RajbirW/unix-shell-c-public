#ifndef VARIABLES_H
#define VARIABLES_H

#include <stddef.h>

typedef struct Variable {
    char *name;
    char *value;
    struct Variable *next;
} Variable;


void set_variable(char *assignment);
char *get_variable(char *name);
void free_tokens(char **tokens);

#endif
