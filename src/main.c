#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
  // Flush after every printf
  setbuf(stdout, NULL);

  char input[100];

  printf("$ ");
  fgets(input, 100, stdin);
  input[strcspn(input, "\n")] = 0;
  printf("%s: command not found\n", input);
  return 0;
}
