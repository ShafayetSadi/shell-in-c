#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/wait.h>

#define INPUT_SIZE 1024
#define MAX_PATH_TOKENS 100
#define MAX_PATH_LENGTH 512
#define MAX_ARGS 100

// Command structure
typedef struct
{
  char *name;
  char **args;
  int arg_count;
} Command;

// Function declarations
void get_input_command(char *input);
void execute_echo(const char *input);
void execute_pwd(const char *input);
void execute_type(const char *command, char **path_tokens, int path_count);
void not_found(const char *command);
void free_path_tokens(char **tokens, int count);
void free_command(Command *cmd);
int check_builtin_command(const char *command);
int find_command_in_path(const char *command, char **path_tokens, int path_count);
int parse_command(const char *input, Command *cmd);
int execute_program(const char *program, char **path_tokens, int path_count, char **args, int arg_count);

void get_input_command(char *input)
{
  if (fgets(input, INPUT_SIZE, stdin) == NULL)
  {
    if (feof(stdin))
    {
      exit(0);
    }
    perror("Error reading input");
    exit(1);
  }
  input[strcspn(input, "\n")] = '\0'; // Remove trailing newline
}

void execute_echo(const char *input)
{
  printf("%s\n", input + 5);
}

void execute_pwd(const char *input)
{
  char cwd[INPUT_SIZE];
  getcwd(cwd, sizeof(cwd));
  printf("%s\n", cwd);
}

void execute_type(const char *command, char **path_tokens, int path_count)
{
  if (!check_builtin_command(command))
  {
    if (!find_command_in_path(command, path_tokens, path_count))
    {
      printf("%s: not found\n", command);
    }
  }
}

void not_found(const char *command)
{
  printf("%s: command not found\n", command);
}

void free_path_tokens(char **tokens, int count)
{
  for (int i = 0; i < count; i++)
  {
    free(tokens[i]);
  }
}

void free_command(Command *cmd)
{
  if (cmd->args != NULL)
  {
    for (int i = 0; i < cmd->arg_count; i++)
    {
      free(cmd->args[i]);
    }
    free(cmd->args);
  }
}

int check_builtin_command(const char *command)
{
  const char *builtin_commands[] = {"echo", "exit", "type", "pwd"};
  const int num_builtins = sizeof(builtin_commands) / sizeof(builtin_commands[0]);

  for (int i = 0; i < num_builtins; i++)
  {
    if (strcmp(builtin_commands[i], command) == 0)
    {
      printf("%s is a shell builtin\n", command);
      return 1;
    }
  }
  return 0;
}

int find_command_in_path(const char *command, char **path_tokens, int path_count)
{
  char fullpath[MAX_PATH_LENGTH];
  struct stat file_stat;

  for (int i = 0; i < path_count; i++)
  {
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path_tokens[i], command);

    if (stat(fullpath, &file_stat) == 0)
    {
      if (S_ISREG(file_stat.st_mode) && (file_stat.st_mode & S_IXUSR))
      {
        printf("%s is %s\n", command, fullpath);
        return 1;
      }
    }
  }
  return 0;
}

int parse_command(const char *input, Command *cmd)
{
  char *input_copy = strdup(input);
  if (input_copy == NULL)
  {
    perror("Memory allocation failed");
    return 0;
  }

  cmd->args = malloc(MAX_ARGS * sizeof(char *));
  if (cmd->args == NULL)
  {
    perror("Memory allocation failed");
    free(input_copy);
    return 0;
  }

  cmd->arg_count = 0;
  char *token = strtok(input_copy, " ");

  while (token != NULL && cmd->arg_count < MAX_ARGS)
  {
    cmd->args[cmd->arg_count] = strdup(token);
    if (cmd->args[cmd->arg_count] == NULL)
    {
      perror("Memory allocation failed");
      free_command(cmd);
      free(input_copy);
      return 0;
    }
    cmd->arg_count++;
    token = strtok(NULL, " ");
  }

  cmd->args[cmd->arg_count] = NULL;
  cmd->name = cmd->args[0];
  free(input_copy);
  return 1;
}

int execute_program(const char *program, char **path_tokens, int path_count, char **args, int arg_count)
{
  pid_t pid = fork();
  if (pid == 0)
  {
    // Child process
    execvp(program, args);
    exit(127);
  }
  else if (pid > 0)
  {
    // Parent process
    int status;
    wait(&status);
    if (WIFEXITED(status))
    {
      int exit_status = WEXITSTATUS(status);
      if (exit_status == 127)
      {
        return 0;
      }
      return 1;
    }
  }
  return 0;
}

int main(int argc, char *argv[])
{
  setbuf(stdout, NULL); // Flush after every printf

  char input[INPUT_SIZE];
  char *path_tokens[MAX_PATH_TOKENS];
  int path_count = 0;

  // Get PATH environment variable
  const char *path = getenv("PATH");
  if (path == NULL)
  {
    fprintf(stderr, "PATH environment variable not set\n");
    return 1;
  }

  // Tokenize PATH
  char *path_copy = strdup(path);
  if (path_copy == NULL)
  {
    perror("Memory allocation failed");
    return 1;
  }

  char *saveptr;
  char *token = strtok_r(path_copy, ":", &saveptr);
  while (token != NULL && path_count < MAX_PATH_TOKENS)
  {
    path_tokens[path_count] = strdup(token);
    if (path_tokens[path_count] == NULL)
    {
      perror("Memory allocation failed");
      free_path_tokens(path_tokens, path_count);
      free(path_copy);
      return 1;
    }
    path_count++;
    token = strtok_r(NULL, ":", &saveptr);
  }
  free(path_copy);

  while (1)
  {
    printf("$ ");
    get_input_command(input);

    if (strlen(input) == 0)
    {
      continue;
    }

    Command cmd = {0};
    if (!parse_command(input, &cmd))
    {
      continue;
    }

    if (strcmp(cmd.name, "exit") == 0)
    {
      free_command(&cmd);
      free_path_tokens(path_tokens, path_count);
      exit(0);
    }
    else if (strcmp(cmd.name, "echo") == 0)
    {
      execute_echo(input);
    }
    else if (strcmp(cmd.name, "pwd") == 0)
    {
      execute_pwd(input);
    }
    else if (strcmp(cmd.name, "type") == 0 && cmd.arg_count > 1)
    {
      execute_type(cmd.args[1], path_tokens, path_count);
    }
    else
    {
      if (!execute_program(cmd.name, path_tokens, path_count, cmd.args, cmd.arg_count))
      {
        not_found(input);
      }
    }

    free_command(&cmd);
  }

  free_path_tokens(path_tokens, path_count);
  return 0;
}
