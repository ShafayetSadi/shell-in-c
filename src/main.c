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
    else
      printf("%s: command not found\n", input);
  }
  return 0;
}
