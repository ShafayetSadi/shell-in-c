#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#define INPUT_SIZE 1024
#define MAX_PATH_TOKENS 100
#define MAX_PATH_LENGTH 512

// Function declarations
void get_input_command(char *input);
void execute_echo(const char *input);
void not_found(const char *command);
void free_path_tokens(char **tokens, int count);
int check_builtin_command(const char *command);
int find_command_in_path(const char *command, char **path_tokens, int path_count);

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

int check_builtin_command(const char *command)
{
  const char *builtin_commands[] = {"echo", "exit", "type"};
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
  for (int i = 0; i < path_count; i++)
  {
    DIR *dir = opendir(path_tokens[i]);
    if (dir == NULL)
    {
      perror("Error opening directory");
      continue;
    }

    struct dirent *entry;
    char fullpath[MAX_PATH_LENGTH];
    struct stat file_stat;

    while ((entry = readdir(dir)) != NULL)
    {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      {
        continue;
      }

      snprintf(fullpath, sizeof(fullpath), "%s/%s", path_tokens[i], entry->d_name);

      if (stat(fullpath, &file_stat) == 0)
      {
        if (S_ISREG(file_stat.st_mode) && (file_stat.st_mode & S_IXUSR))
        {
          if (strcmp(entry->d_name, command) == 0)
          {
            printf("%s is %s\n", command, fullpath);
            closedir(dir);
            return 1;
          }
        }
      }
    }
    closedir(dir);
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

    if (strcmp(input, "exit 0") == 0)
    {
      free_path_tokens(path_tokens, path_count);
      return 0;
    }

    if (strncmp(input, "echo ", 5) == 0)
    {
      execute_echo(input);
    }
    else if (strncmp(input, "type ", 5) == 0)
    {
      const char *command = input + 5;
      if (!check_builtin_command(command))
      {
        if (!find_command_in_path(command, path_tokens, path_count))
        {
          printf("%s: not found\n", command);
        }
      }
    }
    else
    {
      not_found(input);
    }
  }

  free_path_tokens(path_tokens, path_count);
  return 0;
}
