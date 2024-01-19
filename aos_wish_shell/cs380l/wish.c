#include "wish.h"

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

static void close_file(FILE * p_file)
{
    if (NULL != p_file)
    {
        fclose(p_file);
    }
}

static void handle_error(FILE * p_file)
{
    char error_message[30] = "An error has occurred\n"; 
    write(STDERR_FILENO, error_message, strlen(error_message));
    close_file(p_file);
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

int is_built_in (const char * p_command)
{
    param_check(__FILE__, __LINE__, ARG_1, p_command);

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

static char * copy (const char * p_data, size_t length)
{
    param_check(__FILE__, __LINE__, ARG_1, p_data);

    char * p_new = calloc((length + 1), sizeof (char));
    if (NULL == p_new)
    {
        handle_error(NULL);
        exit(1);
    }

    memcpy(p_new, p_data, length);

    return p_new;
}

static size_t getlen (const char * p_str)
{
    const char * p_iter = NULL;
    for (p_iter = p_str; *p_iter; ++p_iter)
    {
        continue;
    }

    return (p_iter - p_str);
}

void execute_builtins (const char * p_command, char ** pp_args, char ** pp_paths)
{
    param_check(__FILE__, __LINE__, ARG_3, p_command, pp_args, pp_paths);

    if (0 == compare(p_command, built_in_commands[0]))
    {
        if (NULL != pp_args[1])
        {
            handle_error(NULL);
            exit(1);
        }

        exit(0);
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
    }
    else if (0 == compare(p_command, built_in_commands[2]))
    {
        for (int i = 0; NULL != pp_paths; i++)
        {
            CLEAR(pp_paths[i]);
        }

        int i = 0;
        while (NULL != pp_args[i + 1])
        {
            size_t arg_sz = getlen(pp_args[i]);
            pp_paths[i] = copy(pp_args[i + 1], arg_sz);
            i++;
        }
        pp_paths[i] = NULL;
    }
}

void parse_input (char * p_input, char ** pp_args)
{
    param_check(__FILE__, __LINE__, ARG_2, p_input, pp_args);

    char * p_token  = strtok(p_input, " \t\n");
    int i = 0;
    while ((NULL != p_token) &&
        ((MAX_ARGS - 1) > i))
    {
        size_t token_sz = getlen(p_token);
        pp_args[i] = copy(p_token, token_sz);
        i++;
        p_token = strtok(NULL, " \t\n");
    }

    pp_args[i] = NULL;
}

void process_command (char * p_input, char ** pp_paths)
{
    param_check(__FILE__, __LINE__, ARG_2, p_input, pp_paths);

    char * pp_args[MAX_ARGS]    = { 0 };
    parse_input(p_input, pp_args);

    if (is_built_in(pp_args[0]))
    {
        execute_builtins(pp_args[0], pp_args, pp_paths);
    }
    else
    {
        char cmd_path[MAX_INPUT]    = { 0 };
        int cmd_found               = 0;

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
                if (-1 == execv(cmd_path, pp_args))
                {
                    handle_error(NULL);
                }
            }
            else if (0 > pid)
            {
                handle_error(NULL);
            }
            else
            {
                int status = 0;
                wait(&status);
            }
        }
        else
        {
            printf("found: %s\n", 1 == cmd_found ? "True" : "False");
            handle_error(NULL);
        }
    }
}

int main (int argc, char * argv[])
{
    if (MAX_EX_ARGS < argc)
    {
        handle_error(NULL);
        exit(1);
    }

    char * p_input = NULL;
    size_t input_sz = 0;
    char * pp_dirs[MAX_ARGS];
    
    size_t dirlen = getlen("/bin");
    pp_dirs[0] = copy("/bin", dirlen);
    pp_dirs[1] = NULL;

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
            if (-1 == bytes_read)
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
            break;
        }

        process_command(p_input, pp_dirs);
    }

    close_file(p_batch);
    
    for (int i = 0; NULL != pp_dirs[i]; i++)
    {
        CLEAR(pp_dirs[i]);
    }

    return 0;
}