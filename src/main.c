#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_SIZE 100

int main(int argc, char *argv[])
{
  setbuf(stdout, NULL); // Flush after every printf

  char input[INPUT_SIZE];

  while (1)
  {
    printf("$ ");
    if (!fgets(input, INPUT_SIZE, stdin))
    {
      perror("\n");
      break;
    }
    input[strcspn(input, "\n")] = 0;

    if (strlen(input) == 0)
      continue;

    if (strncmp(input, "exit", 4) == 0)
      return 0;
    else if (strncmp(input, "echo", 4) == 0)
      printf("%s\n", input + 5);
    else if (strncmp(input, "type", 4) == 0)
    {
      char builtin_commands[][10] = {"echo", "exit", "type", "cd", "pwd", "ls"};
      int is_builtin = 0;
      for (int i = 0; i < sizeof(builtin_commands) / sizeof(builtin_commands[0]); i++)
      {
        if (strcmp(builtin_commands[i], input + 5) == 0)
        {
          printf("%s is a shell builtin\n", input + 5);
          is_builtin = 1;
          break;
        }
      }
      if (!is_builtin)
        printf("%s: not found\n", input + 5);
    }
    else
      printf("%s: command not found\n", input);
  }
  return 0;
}
