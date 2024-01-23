#ifndef __WISH_H__
#define __WISH_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <fcntl.h>
#include <ctype.h>

#define ARG_1   1
#define ARG_2   2
#define ARG_3   3
#define ARG_4   4
#define ARG_5   5

#define MAX_INPUT 1024
#define MAX_EX_ARGS 2
#define MAX_ARGS 64

#define ERR_LOG(idx, file, line, err) fprintf(stderr, "\nERROR:\nparameter %d passed is NULL: %s\n" \
                                "Source: %s - line %d\nProgram exiting ... \n", \
                                idx, strerror(err), file, line)

#define CHECK(idx, file, line, param) do {\
    if (NULL == param) { \
        errno = EINVAL; \
        ERR_LOG(idx, file, line, errno); \
        exit(1); \
    } \
} while(0)

#define CLEAR(a) do {\
    if (a) {\
        free(a); \
        (a)=NULL; \
    } \
} while (0)

void
param_check (const char * fname, int line_no, int n_args, ...);

int
is_built_in (const char * p_command);

void
execute_builtins(const char *p_command, char **pp_args, char **pp_paths);

void
parse_input (char * p_input, char ** pp_paths);

void
process_cmd (char * p_input, char ** pp_paths);

#endif