#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "variables.h"
#include "shell_io.h"

Variable *var_list = NULL;

void set_variable(char *assignment) {
    char *eq_pos = strchr(assignment, '=');
    if (!eq_pos) {
        sh_error("ERROR: Invalid assignment: ", assignment);
        return;
    }

    size_t name_len = (size_t)(eq_pos - assignment);
    char *name = strndup(assignment, name_len);
    char *value = strdup(eq_pos + 1);

    if (!name || !value) {
        sh_error("ERROR: Memory allocation failed: ", assignment);
        free(name);
        free(value);
        return;
    }

    // Expand variables in the value if it contains '$'
    if (strchr(value, '$') != NULL) {
        char *tmp = strdup(value);
        if (!tmp) {
            sh_error("ERROR: Memory allocation failed: ", assignment);
            free(name);
            free(value);
            return;
        }
        char *expanded = get_variable(tmp);
        free(tmp);
        free(value);
        value = expanded;
    }

    Variable *curr = var_list;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            free(curr->value);
            curr->value = value;
            free(name);
            return;
        }
        curr = curr->next;
    }

    Variable *new_var = malloc(sizeof(Variable));
    if (!new_var) {
        sh_error("ERROR: Memory allocation failed: ", assignment);
        free(name);
        free(value);
        return;
    }

    new_var->name = name;
    new_var->value = value;
    new_var->next = var_list;
    var_list = new_var;
}

char *get_variable(char *name) {
    if (!name) return strdup("");

    if (strchr(name, '$') == NULL) {
        Variable *curr = var_list;
        while (curr) {
            if (strcmp(curr->name, name) == 0) {
                return strdup(curr->value);
            }
            curr = curr->next;
        }
        return strdup("");
    }

    char evaluated[SHELL_MAX_LINE];
    evaluated[0] = '\0';

    // strtok mutates input, so this function expects caller to pass a mutable buffer
    char *token = strtok(name, "$");
    while (token != NULL) {
        char *var_value = get_variable(token);
        strncat(evaluated, var_value, sizeof(evaluated) - strlen(evaluated) - 1);
        free(var_value);
        token = strtok(NULL, "$");
    }

    return strdup(evaluated);
}

// Helper function to free all tokens in the token array.
void free_tokens(char **tokens) {
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
        tokens[i] = NULL;
    }
}