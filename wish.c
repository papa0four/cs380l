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

/* a list of native wish shell commands */
const char * built_in_commands[] = {
    "exit",
    "cd",
    "path"
};

void param_check (const char * fname, int line_no, int n_args, ...)
{
    va_list var_list;
    va_start(var_list, n_args);

    for (int i = 0; i < n_args; i++)
    {
        char * param = va_arg(var_list, char *);
        CHECK((i + 1), fname, line_no, param);
    }

    va_end(var_list);
}

/**
 * @brief - a helper function to check a file pointer and close the file. Helps prevent redundant
 *          code blocks when handling errors throughout functions.
 * @param p_file - (FILE *) a pointer to the file being opened
 * @return - (void) does not return any data to the calling function
*/
static void close_file(FILE * p_file)
{
    if (NULL != p_file)
    {
        fclose(p_file);
    }
}

/**
 * @brief - a helper function to handle all errors in accordance with the project description
 * @param p_file - (FILE *) a pointer to the file being opened
 * @return - (void) does not return any data to the calling function
*/
static void handle_error(FILE * p_file)
{
    char error_message[30] = "An error has occurred\n"; 
    write(STDERR_FILENO, error_message, strlen(error_message));
    close_file(p_file);
}

/**
 * @brief - a custom string compare function to use on data passed throughout the program.
 *          does not rely on string functions so can be used on data containing 0's.
 * @param p_str1 - (char *) the first character array used for comparison
 * @param p_str2 - (char *) the seconds character array used to compare against
 * @return - returns 0 if both character arrays are equivalent and -1 upon error
 *           or if the character arrays do not match
*/
static int compare(const char * p_str1, const char * p_str2)
{
    param_check(__FILE__, __LINE__, ARG_2, p_str1, p_str2);

    while(('\0' != *p_str1) &&
    ('\0' != *p_str2))
    {
        if (*p_str1 != *p_str2)
        {
            return -1;
        }

        p_str1++;
        p_str2++;
    }

    if (('\0' == *p_str1) &&
    ('\0' == *p_str2))
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

int is_built_in (const char * p_command)
{
    if (NULL == p_command)
    {
        return 0;
    }

    size_t cmd_sz = sizeof(built_in_commands) / sizeof(built_in_commands[0]);
    for (int i = 0; i < cmd_sz; i++)
    {
        if (0 == compare(p_command, built_in_commands[i]))
        {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief - a custom string copy function to use on data passed throughout the program.
 *          does not rely on string functions so can be used on data containing 0's.
 * @param p_data - (const char *) the character array to be copied
 * @param length - (size_t) the length of the character array being passed
 * @return - (char *) returns a pointer to the newly allocated data to be used
*/
static char * copy (const char * p_data, size_t length)
{
    param_check(__FILE__, __LINE__, ARG_1, p_data);

    char * p_new = calloc((length + 1), sizeof(char));
    if (NULL == p_new)
    {
        handle_error(NULL);
        exit(1);
    }

    memcpy(p_new, p_data, length);

    return p_new;
}

/**
 * @brief - a custom string length function to use on data passed throughout the program.
 *          does not rely on string functions so can be used on data containing 0's.
 * @param p_str - (const char *) a point to the character array to determine its length
 * @return - (size_t) returns the byte count of the array passed determining it's length
*/
static size_t getlen (const char * p_str)
{
    const char * p_iter = NULL;
    for (p_iter = p_str; *p_iter; ++p_iter)
    {
        continue;
    }

    return (p_iter - p_str);
}

/**
 * @brief - if the command passed to the wish shell is found to be a `built-in`, this function
 *          will execute it appropriately.
 * @param p_command - (char *) a pointer to the command that was passed to the wish shell
 * @param pp_args - (char **) a pointer array of arguments passed to the wish shell
 * @param pp_paths - (char **) a pointer array of paths to determine command location within the os 
 *                  file system for execution.
 * @return - (void) does not return any data to the calling function
*/
static void execute_builtins (char * p_command, char ** pp_args, char ** pp_paths)
{
    param_check(__FILE__, __LINE__, ARG_3, p_command, pp_args, pp_paths);

    if (0 == compare(p_command, built_in_commands[0]))
    {
        if (NULL != pp_args[1])
        {
            handle_error(NULL);
        }

        for (int i = 0; (i < MAX_ARGS) && (NULL != pp_args[i]); i++)
        {
            CLEAR(pp_args[i]);
        }
    }
    else if (0 == compare(p_command, built_in_commands[1]))
    {
        if (NULL != pp_args[1])
        {
            if (0 != chdir(pp_args[1]))
            {
                handle_error(NULL);
            }
        }
        else
        {
            handle_error(NULL);
        }
    }
    else if (0 == compare(p_command, built_in_commands[2]))
    {
        for (int i = 0; NULL != pp_paths[i]; i++)
        {
            CLEAR(pp_paths[i]);
        }

        int i = 0;
        while (NULL != pp_args[i + 1])
        {
            size_t arg_sz = getlen(pp_args[i + 1]);
            pp_paths[i] = copy(pp_args[i + 1], arg_sz);
            i++;
        }
        pp_paths[i] = NULL;
    }
}

/**
 * @brief - handles the input passed to the wish shell and tokenizes the data for ease of processing
 * @param p_input - (char *) a pointer to the input passed to the wish shell
 * @param pp_paths - (char **) a pointer array of paths to determine command location within the os 
 *                  fie system for execution.
 * @return (void) does not return any data to the calling function
*/
static void parse_input (char * p_input, char ** pp_args)
{
    param_check(__FILE__, __LINE__, ARG_2, p_input, pp_args);

    char * saveptr  = NULL;
    char * p_token  = strtok_r(p_input, " \t\n", &saveptr);
    int i           = 0;

    while ((NULL != p_token) &&
        ((MAX_ARGS - 1) > i))
    {
        char * p_redirect = strchr(p_token, '>');
        size_t token_sz = ((NULL != p_redirect) ? (p_redirect - p_token) : getlen(p_token));
        if (0 < token_sz)
        {
            pp_args[i] = copy(p_token, token_sz);
            if (NULL == pp_args[i])
            {
                for (int j = 0; j < i; j++)
                {
                    CLEAR(pp_args[j]);
                }
                return;
            }
            i++;
        }
        if (NULL != p_redirect)
        {
            pp_args[i] = copy(p_redirect, 1);
            i++;
            p_token = p_redirect + 1;
        }
        else 
        {
            p_token = strtok_r(NULL, " \t\n", &saveptr);
        }
    }

    pp_args[i] = NULL;
}

/**
 * @brief - executes all redirect and non-native commands passed to the wish shell.
 *          This function was primarily to clean up the process_command function and 
 *          extract functionality out for potential reuse.
 * @param redir_out - (int) an integer toggle to determine if the command passed was a
 *                    redirector operation.
 * @param p_file - (char *) a character array containing the redirection file name if 
 *                 redirection operation occurs.
 * @param pp_paths - (char **) - a pointer array of paths to determine command location within the os 
 *                  fie system for execution.
 * @param pp_args - (char **) a pointer array of arguments passed to the wish shell
 * @return - (void) does not return any data to the calling function
*/
static void execute_cmd (int redir_out, char * p_file, char ** pp_paths, char ** pp_args)
{
    param_check(__FILE__, __LINE__, ARG_2, pp_paths, pp_args);

    char cmd_path[MAX_INPUT]    = { 0 };
    int  cmd_found              = 0;

    for (int i = 0; NULL != pp_paths[i]; i++)
    {
        snprintf(cmd_path, sizeof(cmd_path), "%s/%s", pp_paths[i], pp_args[0]);

        if (0 == access(cmd_path, X_OK))
        {
            cmd_found = 1;
            break;
        }
    }

    if (cmd_found)
    {
        pid_t pid = fork();
        if (0 == pid)
        {
            if (redir_out)
            {
                int fd = open(p_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (-1 == fd)
                {
                    handle_error(NULL);
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            if (-1 == execv(cmd_path, pp_args))
            {
                handle_error(NULL);
                exit(1);
            }
        }
        else if (0 > pid)
        {
            handle_error(NULL);
            exit(1);
        }
    }
    else
    {
        handle_error(NULL);
    }
}

/**
 * @brief - a helper function that takes the parsed user input and determines the line of execution
 *          based upon the type of command being passed. Native/Non-native, redirection, and/or
 *          parallel commands are processed here helping the wish shell function according to the
 *          program's guidance documentation.
 * @param p_input - (char *) a pointer to the command passed to the wish shell
 * @param pp_paths - (char **) a pointer array of paths to determine command location within the os 
 *                  fie system for execution.
 * @return - (void) does not return any data to the calling function
*/
static void process_command (char * p_input, char ** pp_paths)
{
    param_check(__FILE__, __LINE__, ARG_2, p_input, pp_paths);

    char ** pp_args = calloc(MAX_ARGS, sizeof(char *));
    if (NULL == pp_args)
    {
        handle_error(NULL);
        exit(1);
    }

    parse_input(p_input, pp_args);

    int     redirect_output     = 0;
    char  * p_out_file          = NULL;

    for (int i = 0; NULL != pp_args[i]; i++)
    {
        if (0 == compare(pp_args[i], ">"))
        {
            if ((0 < i) &&
                (NULL != pp_args[i + 1]) &&
                (NULL == pp_args[i + 2]))
            {
                size_t filename_len = getlen(pp_args[i + 1]);
                p_out_file = copy(pp_args[i + 1], filename_len);
                redirect_output = 1;
                CLEAR(pp_args[i]);
                CLEAR(pp_args[i+1]);
                break;
            }
            else
            {
                handle_error(NULL);
                for (int j = 0; (j < MAX_ARGS) && (NULL != pp_args[j]); j++)
                {
                    CLEAR(pp_args[j]);
                }

                CLEAR(pp_args);
                CLEAR(p_out_file);
                return;
            }
        }
    }

    if (NULL == pp_args[0])
    {
        // empty command; do nothing
    }
    else if (is_built_in(pp_args[0]))
    {
        execute_builtins(pp_args[0], pp_args, pp_paths);
    }
    else
    {
        execute_cmd(redirect_output, p_out_file, pp_paths, pp_args);
    }

    if (redirect_output)
    {
        if (NULL != p_out_file)
        {
            CLEAR(p_out_file);
        }
    }

    for (int i = 0; (i < MAX_ARGS) && (NULL != pp_args[i]); i++)
    {
        CLEAR(pp_args[i]);
    }
    CLEAR(pp_args);
}

void process_line(char * p_input, char ** pp_paths)
{
    char * saveptr = NULL;
    char * p_command = strtok_r(p_input, "&", &saveptr);
    while (NULL != p_command)
    {
        process_command(p_command, pp_paths);
        p_command = strtok_r(NULL, "&", &saveptr);
    }
    int status = 0;
    pid_t cpid;
    while ((cpid = wait(&status)) > 0);
}

int main (int argc, char * argv[])
{
    if (MAX_EX_ARGS < argc)
    {
        handle_error(NULL);
        exit(1);
    }

    char    * p_input           = NULL;
    size_t    input_sz          = 0;
    char    * pp_dirs[MAX_ARGS] = { 0 };
    
    size_t dirlen   = getlen("/bin");
    pp_dirs[0]      = copy("/bin", dirlen);
    pp_dirs[1]      = NULL;

    FILE * p_batch = NULL;
    if (2 == argc)
    {
        p_batch = fopen(argv[1], "r");
        if (NULL == p_batch)
        {
            handle_error(NULL);
            exit(1);
        }
    }
    
    while(1)
    {
        if ((1 == argc) &&
            (0 == compare(argv[0], "./wish")))
        {
            printf("\nwish> ");
            ssize_t bytes_read = getline(&p_input, &input_sz, stdin);
            if ((-1 == bytes_read) ||
                (feof(stdin)))
            {
                break;
            }
        }
        else if ((2 == argc) &&
                (0 == compare(argv[0], "./wish")))
        {
            ssize_t bytes_read = getline(&p_input, &input_sz, p_batch);
            if ((-1 == bytes_read) ||
                (feof(p_batch)))
            {
                break;
            }
        }
        else
        {
            handle_error(NULL);
        }
        process_line(p_input, pp_dirs);

        if (0 == compare(built_in_commands[0], p_input))
        {
            break;
        }
        
        CLEAR(p_input);
    }

    close_file(p_batch);

    CLEAR(p_input);
    
    for (int i = 0; NULL != pp_dirs[i]; i++)
    {
        CLEAR(pp_dirs[i]);
    }

    return 0;
}