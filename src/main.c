#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>

#define INPUT_SIZE 1024
#define MAX_PATH_TOKENS 100
#define MAX_PATH_LENGTH 512
#define MAX_ARGS 100
#define INITIAL_EXEC_CAPACITY 256

char **all_commands = NULL;

typedef struct
{
  char *name;
  char **args;
  int arg_count;
} Command;

typedef enum
{
  REDIRECT_NONE,
  REDIRECT_STDOUT,        // >
  REDIRECT_STDOUT_APPEND, // >>
  REDIRECT_STDERR,        // 2>
  REDIRECT_STDERR_APPEND  // 2>>
} RedirectionType;

typedef struct
{
  RedirectionType type;
  char *filepath;
  int operator_index;
} Redirection;

// Function declarations
void execute_echo(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir);
void execute_pwd(const Command *cmd, bool isRedirect);
void execute_cd(const char *target_dir);
void execute_type(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir);
void not_found(const char *command);
void free_path_tokens(char **tokens, int count);
void free_command(Command *cmd);
int check_builtin_command(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir);
int find_command_in_path(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir);
int parse_command(const char *input, Command *cmd);
int execute_program(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir, const char *input);
void print_debug_info(const Command *cmd);
void execute_command(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir);
Redirection parse_redirection(const Command *cmd);
char *command_generator(const char *text, int state);
char **my_completion(const char *text, int start, int end);
static int is_in_array(char **array, int count, const char *name);
char **get_executables_from_path();

static int is_in_array(char **array, int count, const char *name)
{
  for (int i = 0; i < count; i++)
  {
    if (strcmp(array[i], name) == 0)
      return 1;
  }
  return 0;
}

char **get_executables_from_path()
{
  const char *path = getenv("PATH");
  if (!path)
    return NULL;

  char *path_copy = strdup(path);

  int exec_capacity = INITIAL_EXEC_CAPACITY;
  int exec_count = 0;
  char **executables = malloc(exec_capacity * sizeof(char *));
  if (!executables)
  {
    free(path_copy);
    return NULL;
  }

  char *saveptr;
  char *dir_path = strtok_r(path_copy, ":", &saveptr);
  while (dir_path)
  {
    DIR *dir = opendir(dir_path);
    if (dir)
    {
      struct dirent *entry;
      while ((entry = readdir(dir)) != NULL)
      {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
          continue;

        // Check for duplicates
        if (is_in_array(executables, exec_count, entry->d_name))
          continue;

        // Build full path
        char fullpath[MAX_PATH_LENGTH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode) &&
            (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
        {
          // Add to array
          if (exec_count == exec_capacity)
          {
            exec_capacity *= 2;
            char **tmp = realloc(executables, exec_capacity * sizeof(char *));
            if (!tmp)
              break;
            executables = tmp;
          }
          executables[exec_count++] = strdup(entry->d_name);
        }
      }
      closedir(dir);
    }
    dir_path = strtok_r(NULL, ":", &saveptr);
  }
  free(path_copy);

  // Null-terminate the array
  if (exec_count == exec_capacity)
  {
    char **tmp = realloc(executables, (exec_capacity + 1) * sizeof(char *));
    if (tmp)
      executables = tmp;
  }
  executables[exec_count] = NULL;
  return executables;
}

void execute_echo(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir)
{
  int pipeline_index = -1;
  for (int i = 0; i < cmd->arg_count; i++)
  {
    if (strcmp(cmd->args[i], "|") == 0)
    {
      pipeline_index = i;
      break;
    }
  }

  if (pipeline_index != -1)
  {
    char input[INPUT_SIZE] = {0};
    for (int i = 1; i < pipeline_index; i++)
    {
      strcat(input, cmd->args[i]);
      strcat(input, " ");
    }
    if (strlen(input) > 0 && input[strlen(input) - 1] == ' ')
    {
      input[strlen(input) - 1] = '\n';
      input[strlen(input)] = '\0';
    }

    Command new_cmd = {0};

    new_cmd.args = malloc(MAX_ARGS * sizeof(char *));
    new_cmd.arg_count = cmd->arg_count - pipeline_index - 1;

    for (int i = 0; i < new_cmd.arg_count; i++)
    {
      new_cmd.args[i] = cmd->args[pipeline_index + i + 1];
    }
    new_cmd.args[new_cmd.arg_count] = NULL;
    new_cmd.name = new_cmd.args[0];

    if (execute_program(&new_cmd, path_tokens, path_count, redir, input))
    {
      not_found(cmd->name);
    }

    return;
  }

  if (redir->type == REDIRECT_STDOUT)
    freopen(redir->filepath, "w", stdout);
  else if (redir->type == REDIRECT_STDOUT_APPEND)
    freopen(redir->filepath, "a", stdout);
  else if (redir->type == REDIRECT_STDERR)
    freopen(redir->filepath, "w", stderr);
  else if (redir->type == REDIRECT_STDERR_APPEND)
    freopen(redir->filepath, "a", stderr);

  // Calculate the end index based on whether there's redirection
  int end_index = redir->type != REDIRECT_NONE ? redir->operator_index : cmd->arg_count;

  for (int i = 1; i < end_index; i++)
  {
    printf("%s ", cmd->args[i]);
  }
  printf("\n");

  fflush(stdout);
  freopen("/dev/tty", "w", stdout);
}

void execute_pwd(const Command *cmd, bool isRedirect)
{
  char cwd[INPUT_SIZE];
  if (getcwd(cwd, sizeof(cwd)) != NULL)
  {
    if (isRedirect)
    {
      char *filepath = cmd->args[cmd->arg_count - 1];
      FILE *file = fopen(filepath, "w");
      if (file == NULL)
      {
        perror("Error opening file");
        return;
      }
      fprintf(file, "%s\n", cwd);
      fclose(file);
    }
    else
    {
      printf("%s\n", cwd);
    }
  }
}

void execute_cd(const char *target_dir)
{
  const char *dir = target_dir;
  if (target_dir == NULL || strcmp(target_dir, "~") == 0)
  {
    dir = getenv("HOME");
    if (dir == NULL)
    {
      return;
    }
  }

  if (chdir(dir) != 0)
  {
    printf("cd: %s: No such file or directory\n", dir);
  }
}

void execute_type(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir)
{
  if (!check_builtin_command(cmd, path_tokens, path_count, redir))
  {
    if (!find_command_in_path(cmd, path_tokens, path_count, redir))
    {
      printf("%s: not found\n", cmd->args[1]);
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

int check_builtin_command(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir)
{
  const char *builtin_commands[] = {"echo", "exit", "type", "pwd", "cd"};
  const int num_builtins = sizeof(builtin_commands) / sizeof(builtin_commands[0]);

  bool isBuiltin = false;

  int pipeline_index = -1;
  for (int i = 0; i < cmd->arg_count; i++)
  {
    if (strcmp(cmd->args[i], "|") == 0)
    {
      pipeline_index = i;
      break;
    }
  }

  if (pipeline_index != -1)
  {
    char input[INPUT_SIZE] = {0};
    for (int i = 0; i < num_builtins; i++)
    {
      if (strcmp(builtin_commands[i], cmd->args[1]) == 0)
      {
        snprintf(input, INPUT_SIZE, "%s is a shell builtin", cmd->args[1]);
        isBuiltin = true;
        break;
      }
    }
    if (!isBuiltin)
      return 0;

    for (int i = 1; i < pipeline_index; i++)
    {
      strcat(input, cmd->args[i]);
      strcat(input, " ");
    }
    if (strlen(input) > 0 && input[strlen(input) - 1] == ' ')
      input[strlen(input) - 1] = '\0';

    Command new_cmd = {0};

    new_cmd.args = malloc(MAX_ARGS * sizeof(char *));
    new_cmd.arg_count = cmd->arg_count - pipeline_index - 1;

    for (int i = 0; i < new_cmd.arg_count; i++)
    {
      new_cmd.args[i] = cmd->args[pipeline_index + i + 1];
    }
    new_cmd.args[new_cmd.arg_count] = NULL;
    new_cmd.name = new_cmd.args[0];

    if (execute_program(&new_cmd, path_tokens, path_count, redir, input))
    {
      not_found(cmd->name);
    }

    return 1;
  }

  bool isRedirect = (redir->type != REDIRECT_NONE);

  FILE *file = NULL;
  if (isRedirect)
  {
    char *filepath = cmd->args[cmd->arg_count - 1];
    file = fopen(filepath, "w");
    if (file == NULL)
    {
      perror("Error opening file");
      return 0;
    }
  }
  for (int i = 0; i < num_builtins; i++)
  {
    if (strcmp(builtin_commands[i], cmd->args[1]) == 0)
    {
      if (isRedirect)
        fprintf(file, "%s is a shell builtin\n", cmd->args[1]);
      else
        printf("%s is a shell builtin\n", cmd->args[1]);
      isBuiltin = true;
      break;
    }
  }
  if (isRedirect && file != NULL)
  {
    fclose(file);
  }
  if (isBuiltin)
    return 1;
  return 0;
}

int find_command_in_path(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir)
{
  bool isRedirect = (redir->type != REDIRECT_NONE);
  char fullpath[MAX_PATH_LENGTH];
  struct stat file_stat;
  FILE *file = NULL;
  bool cmdFound = false;

  int pipeline_index = -1;
  for (int i = 0; i < cmd->arg_count; i++)
  {
    if (strcmp(cmd->args[i], "|") == 0)
    {
      pipeline_index = i;
      break;
    }
  }

  if (pipeline_index != -1)
  {
    char input[INPUT_SIZE] = {0};
    for (int i = 0; i < path_count; i++)
    {
      snprintf(fullpath, sizeof(fullpath), "%s/%s", path_tokens[i], cmd->args[1]);

      if (stat(fullpath, &file_stat) == 0)
      {
        if (S_ISREG(file_stat.st_mode) && (file_stat.st_mode & S_IXUSR))
        {
          snprintf(input, INPUT_SIZE, "%s is %s\n", cmd->args[1], fullpath);
          cmdFound = true;
          break;
        }
      }
    }

    if (!cmdFound)
      return 0;

    Command new_cmd = {0};

    new_cmd.args = malloc(MAX_ARGS * sizeof(char *));
    new_cmd.arg_count = cmd->arg_count - pipeline_index - 1;

    for (int i = 0; i < new_cmd.arg_count; i++)
    {
      new_cmd.args[i] = cmd->args[pipeline_index + i + 1];
    }
    new_cmd.args[new_cmd.arg_count] = NULL;
    new_cmd.name = new_cmd.args[0];

    if (execute_program(&new_cmd, path_tokens, path_count, redir, input))
    {
      not_found(cmd->name);
    }
    if (cmdFound)
      return 1;
    return 0;
  }

  if (isRedirect)
  {
    char *filepath = cmd->args[cmd->arg_count - 1];
    file = fopen(filepath, "w");
    if (file == NULL)
    {
      perror("Error opening file");
      return 0;
    }
  }

  for (int i = 0; i < path_count; i++)
  {
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path_tokens[i], cmd->args[1]);

    if (stat(fullpath, &file_stat) == 0)
    {
      if (S_ISREG(file_stat.st_mode) && (file_stat.st_mode & S_IXUSR))
      {
        if (isRedirect)
          fprintf(file, "%s is %s\n", cmd->args[1], fullpath);
        else
          printf("%s is %s\n", cmd->args[1], fullpath);
        cmdFound = true;
        break;
      }
    }
  }

  if (isRedirect && file != NULL)
  {
    fclose(file);
  }

  if (cmdFound)
    return 1;
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
  char *current = input_copy;

  while (*current && cmd->arg_count < MAX_ARGS)
  {
    while (*current == ' ')
      current++;
    if (!*current)
      break;

    char *start = current;
    char *arg_start = current;
    char quote = '\'';
    int in_quotes = 0;

    while (*current && (*current != ' ' || in_quotes))
    {
      if ((*current == '\'' || *current == '\"') && !in_quotes)
      {
        quote = *current;
      }

      if (*current == quote)
      {
        if (!in_quotes)
        {
          // Start: quoted string
          in_quotes = 1;
          memmove(current, current + 1, strlen(current));
        }
        else
        {
          // End: quoted string
          in_quotes = 0;
          memmove(current, current + 1, strlen(current));
        }
      }
      else
      {
        if (*current == '\\')
        {
          if (!in_quotes)
          {
            memmove(current, current + 1, strlen(current));
          }
          else if (quote == '\"' && (*(current + 1) == '\\' || *(current + 1) == '\"'))
          {
            memmove(current, current + 1, strlen(current));
          }
        }
        current++;
      }
    }

    if (*current)
    {
      *current = '\0';
      current++;
    }

    cmd->args[cmd->arg_count] = strdup(arg_start);
    if (cmd->args[cmd->arg_count] == NULL)
    {
      perror("Memory allocation failed");
      free_command(cmd);
      free(input_copy);
      return 0;
    }
    cmd->arg_count++;
  }

  cmd->args[cmd->arg_count] = NULL;
  cmd->name = cmd->args[0];
  free(input_copy);
  return 1;
}

int execute_program(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir, const char *input)
{
  int pipeline_index = -1;
  for (int i = 0; i < cmd->arg_count; i++)
  {
    if (strcmp(cmd->args[i], "|") == 0)
    {
      pipeline_index = i;
      break;
    }
  }

  int pipefds[2];
  // No pipeline: just handle redirection and exec
  if (pipeline_index == -1)
  {
    if (input != NULL)
    {
      pipe(pipefds);
    }

    pid_t pid = fork();
    if (pid == 0)
    {
      // Child process
      if (redir->type != REDIRECT_NONE)
      {
        char *filepath = redir->filepath;
        int new_arg_count = 0;
        char **new_args = malloc((cmd->arg_count + 1) * sizeof(char *));

        if (new_args == NULL)
        {
          perror("Memory allocation failed");
          exit(1);
        }

        // Copy arguments until redirection operator
        for (int i = 0; i < redir->operator_index; i++)
        {
          new_args[new_arg_count++] = cmd->args[i];
        }
        new_args[new_arg_count] = NULL;

        if (filepath != NULL)
        {
          int flags = O_WRONLY | O_CREAT;
          if (redir->type == REDIRECT_STDOUT || redir->type == REDIRECT_STDERR)
            flags |= O_TRUNC;
          else
            flags |= O_APPEND;

          int fd = open(filepath, flags, 0644);
          if (fd == -1)
          {
            perror("Error opening file");
            free(new_args);
            exit(1);
          }
          int target_fd = (redir->type == REDIRECT_STDOUT || redir->type == REDIRECT_STDOUT_APPEND) ? STDOUT_FILENO : STDERR_FILENO;
          if (dup2(fd, target_fd) == -1)
          {
            perror("Error redirecting output");
            close(fd);
            free(new_args);
            exit(1);
          }
          close(fd);
        }

        // Execute the program with modified arguments
        execvp(cmd->name, new_args);
        free(new_args);
        exit(127);
      }
      else
      {
        if (input != NULL)
        {
          close(pipefds[1]);              // Close write end
          dup2(pipefds[0], STDIN_FILENO); // Redirect stdin to read end
          close(pipefds[0]);

          execvp(cmd->name, cmd->args);
          perror("execvp failed");
          exit(1);
        }
        else
        {
          execvp(cmd->name, cmd->args);
          exit(127);
        }
      }
    }
    else if (pid > 0)
    {
      if (input != NULL)
      {
        close(pipefds[0]); // Close read end

        // Write input to less
        write(pipefds[1], input, strlen(input));
        close(pipefds[1]); // Close write end to signal EOF
      }
      // Parent process
      int status;
      wait(&status);
      if (WIFEXITED(status))
      {
        int exit_status = WEXITSTATUS(status);
        if (exit_status == 127)
        {
          return 1; // Command not found
        }
        return 0; // Command found, but failed
      }
    }
    return 1;
  }

  // Pipeline: cmd1 | cmd2
  if (pipe(pipefds) == -1)
  {
    perror("Pipe failed");
    return 1;
  }

  char *args1[pipeline_index + 1];
  for (int i = 0; i < pipeline_index; i++)
    args1[i] = cmd->args[i];
  args1[pipeline_index] = NULL;

  int args2_count = cmd->arg_count - pipeline_index - 1;
  char *args2[args2_count + 1];
  for (int i = 0; i < args2_count; i++)
    args2[i] = cmd->args[pipeline_index + 1 + i];
  args2[args2_count] = NULL;

  pid_t pid1 = fork();
  if (pid1 == 0)
  {
    // First child: left side of pipe
    close(pipefds[0]);               // Close unused read end
    dup2(pipefds[1], STDOUT_FILENO); // Redirect stdout to pipe
    close(pipefds[1]);
    execvp(args1[0], args1);
    perror("execvp (left) failed");
    exit(127);
  }

  pid_t pid2 = fork();
  if (pid2 == 0)
  {
    // Second child: right side of pipe
    close(pipefds[1]);              // Close unused write end
    dup2(pipefds[0], STDIN_FILENO); // Redirect stdin to pipe
    close(pipefds[0]);

    if (strcmp(args2[0], "type") == 0)
    {
      Command builtin_cmd = {.name = args2[0], .args = args2, .arg_count = args2_count};
      Redirection dummy_redir = {REDIRECT_NONE, NULL, -1};
      execute_type(&builtin_cmd, path_tokens, path_count, &dummy_redir);
      exit(0);
    }

    execvp(args2[0], args2);
    perror("execvp (right) failed");
    exit(127);
  }

  // Parent process
  close(pipefds[0]);
  close(pipefds[1]);

  int status1, status2;
  waitpid(pid1, &status1, 0);
  waitpid(pid2, &status2, 0);

  // Return 1 if either side of the pipe was not found (exit code 127), else 0
  if ((WIFEXITED(status1) && WEXITSTATUS(status1) == 127) ||
      (WIFEXITED(status2) && WEXITSTATUS(status2) == 127))
  {
    return 1;
  }
  return 0;
}

void print_debug_info(const Command *cmd)
{
  printf("CMD: %s\n", cmd->name);
  printf("ARGS: |");
  for (int i = 0; i < cmd->arg_count; i++)
  {
    printf(" %s |", cmd->args[i]);
  }
  printf("\n");
}

Redirection parse_redirection(const Command *cmd)
{
  Redirection redir = {REDIRECT_NONE, NULL, -1};
  for (int i = 0; i < cmd->arg_count; i++)
  {
    if (strcmp(cmd->args[i], ">") == 0 || strcmp(cmd->args[i], "1>") == 0)
    {
      redir.type = REDIRECT_STDOUT;
      redir.operator_index = i;
      break;
    }
    else if (strcmp(cmd->args[i], "2>") == 0)
    {
      redir.type = REDIRECT_STDERR;
      redir.operator_index = i;
      break;
    }
    else if (strcmp(cmd->args[i], "1>>") == 0 || strcmp(cmd->args[i], ">>") == 0)
    {
      redir.type = REDIRECT_STDOUT_APPEND;
      redir.operator_index = i;
      break;
    }
    else if (strcmp(cmd->args[i], "2>>") == 0)
    {
      redir.type = REDIRECT_STDERR_APPEND;
      redir.operator_index = i;
      break;
    }
  }

  if (redir.type != REDIRECT_NONE)
  {
    if (redir.operator_index + 1 < cmd->arg_count)
    {
      redir.filepath = cmd->args[redir.operator_index + 1];
    }
  }

  return redir;
}

char *command_generator(const char *text, int state)
{
  static int list_index;
  static int exec_index;
  static int mode; // 0 = builtins, 1 = executables
  static const char *builtins[] = {"echo", "exit", "type", "pwd", "cd", NULL};

  if (state == 0)
  {
    list_index = 0;
    exec_index = 0;
    mode = 0;
  }

  if (mode == 0)
  {
    while (builtins[list_index])
    {
      const char *cmd = builtins[list_index++];
      if (strncmp(cmd, text, strlen(text)) == 0)
      {
        return strdup(cmd);
      }
    }
    mode = 1;
  }

  if (mode == 1 && all_commands)
  {
    while (all_commands[exec_index])
    {
      const char *cmd = all_commands[exec_index++];
      if (strncmp(cmd, text, strlen(text)) == 0)
        return strdup(cmd);
    }
  }

  return NULL;
}

char **my_completion(const char *text, int start, int end)
{
  rl_attempted_completion_over = 1;
  return rl_completion_matches(text, command_generator);
}

void execute_command(const Command *cmd, char **path_tokens, int path_count, const Redirection *redir)
{
  bool isRedirect = redir->type == REDIRECT_STDOUT;
  if (strcmp(cmd->name, "exit") == 0)
  {
    free_command((Command *)cmd);
    free_path_tokens(path_tokens, path_count);
    exit(0);
  }
  else if (strcmp(cmd->name, "echo") == 0)
  {
    execute_echo(cmd, path_tokens, path_count, redir);
  }
  else if (strcmp(cmd->name, "pwd") == 0)
  {
    execute_pwd(cmd, isRedirect);
  }
  else if (strcmp(cmd->name, "cd") == 0)
  {
    execute_cd(cmd->arg_count > 1 ? cmd->args[1] : NULL);
  }
  else if (strcmp(cmd->name, "type") == 0 && cmd->arg_count > 1)
  {
    execute_type(cmd, path_tokens, path_count, redir);
  }
  else
  {
    if (execute_program(cmd, path_tokens, path_count, redir, NULL))
    {
      not_found(cmd->name);
    }
  }
}

int main(int argc, char *argv[])
{
  setbuf(stdout, NULL); // Flush after every printf
  rl_attempted_completion_function = my_completion;
  rl_bind_key('\t', rl_complete);
  all_commands = get_executables_from_path();

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

  char *input;
  while ((input = readline("$ ")) != NULL)
  {
    if (*input)
      add_history(input);

    Command cmd = {0};
    if (!parse_command(input, &cmd))
    {
      continue;
    }
    // print_debug_info(&cmd);

    Redirection redir = parse_redirection(&cmd);
    execute_command(&cmd, path_tokens, path_count, &redir);
    free_command(&cmd);
    free(input);
  }

  free_path_tokens(path_tokens, path_count);
  free(all_commands);
  return 0;
}
