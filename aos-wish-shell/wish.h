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

/* helper macros for the CHECK macro */
#define ARG_1   1
#define ARG_2   2
#define ARG_3   3
#define ARG_4   4
#define ARG_5   5

/* pre-defined values to avoid "magic number usage" */
#define MAX_INPUT 1024
#define MAX_EX_ARGS 2
#define MAX_ARGS 64

/* skeleton macro for parameter check */
#define ERR_LOG(idx, file, line, err) fprintf(stderr, "\nERROR:\nparameter %d passed is NULL: %s\n" \
                                "Source: %s - line %d\nProgram exiting ... \n", \
                                idx, strerror(err), file, line)

/**
 * @brief - a helper macro that determines invalid pointer parameters passed to any function.
 *          The purpose of this macro is to help the user determine the specific parameter
 *          passed that was NULL, which file (if multiple src files), and at which line,
 *          the error occurred.
*/
#define CHECK(idx, file, line, param) do {\
    if (NULL == param) { \
        errno = EINVAL; \
        ERR_LOG(idx, file, line, errno); \
        exit(1); \
    } \
} while(0)

/**
 * @brief - this macro helps clear any heap-allocated memory and sets the pointer to NULL.
 *          It is primarily meant to prevent redundant calls to `free(x); x = NULL;` in 
 *          the code.
*/
#define CLEAR(a) do {\
    if (a) {\
        free(a); \
        (a)=NULL; \
    } \
} while (0)

/* wish shell code */

/**
 * @brief - this is the declaration of the parameter checker which utilizes both CHECK
 *          and ERR_LOG macros.
 * @param fname - (const char *) a pointer to the name of the file calling the function
 * @param line_no - (int) the line number where the parameter check occurs and FAILS
 * @param n_args - (int) the number of parameters passed to the function
 * @param ... - variadic parameter declaration allowing for a dynamic number of parameters
 * @return - (void) does not return any data to the calling function
*/
void
param_check (const char * fname, int line_no, int n_args, ...);

/**
 * @brief - a checking function to determine if the command passed to the wish shell 
 *          is `built-in` or non-native to the wish environment.
 * @param p_command - (const char *) a pointer to the command passed to the wish shell
 * @return - (int) if the command passed to the wish shell matches a command pre-defined
 *          to be built-in, this function will return a value of 1, otherwise it will
 *          return a value of 0.
*/
int
is_built_in (const char * p_command);

/**
 * @brief - handles the various ways to process the data being passed to the wish shell.
 *          internally calls the static process_command function which handles all of the
 *          built-in and non-native "wish shell" commands after parsing the user input.
 * @param p_input - (char *) a pointer to the command being passed by the user
 * @param pp_paths - (char **) a pointer array of paths to determine command location within the os
 *                  file system for execution.
 * @return - (void) does not return any data to the calling function
*/
void 
process_line(char * p_input, char ** pp_paths);

#endif
