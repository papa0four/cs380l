#include "wish.h"

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

void parse_input
(
    char     * p_input,
    char    ** pp_args,
    int      * p_argc
)
{
    char * p_token  = NULL;
    *p_argc         = 0;

    p_token = strtok(p_input, " \t\n");
    while (NULL != p_token)
    {
        if ((MAX_ARGS - 1) > *p_argc)
        {
            pp_args[*p_argc] = p_token;
            (*p_argc)++;
        }
        else
        {
            fprintf(stderr, "Too many arguments.\n");
            break;
        }
        p_token = strtok(NULL, " \t\n");
    }
    pp_args[*p_argc] = NULL;
}

void process_command (char ** pp_args)
{
    pid_t pid = fork();
    if (0 == pid)
    {
        if (-1 == execvp(pp_args[0], pp_args))
        {
            perror("execvp");
            exit(1);
        }
    }
    else if (0 > pid)
    {
        perror("fork");
        exit(1);
    }
    else
    {
        int status;
        wait(&status);
    }
}

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

int main (int argc, char * argv[])
{
    char      input[MAX_INPUT] = { 0 };
    char    * p_args[MAX_ARGS] = { 0 };
    int       arg_count        = 0;

    FILE * p_batch = NULL;
    if (2 == argc)
    {
        p_batch = fopen(argv[1], "r");
        if (NULL == p_batch)
        {
            fprintf(stderr, "Error opening batch file: %s\n", argv[1]);
            exit(1);
        }
    }

    while(1)
    {
        if (1 == argc)
        {
            printf("\nwish> ");
            if (NULL == fgets(input, MAX_INPUT, stdin))
            {
                break;
            }
        }
        else
        {
            if (NULL == fgets(input, MAX_INPUT, p_batch))
            {
                break;
            }
        }

        if ('\n' == input[strlen(input) - 1])
        {
            input[strlen(input) - 1] = '\0';
        }

        if (0 == compare(input, "exit"))
        {
            break;
        }

        parse_input(input, p_args, &arg_count);
        process_command(p_args);
    }

    if (NULL != p_batch)
    {
        fclose(p_batch);
    }

    return 0;
}