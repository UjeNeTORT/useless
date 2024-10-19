#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// typedef unsigned long long size_t;

const size_t MAX_COMMAND = 1 << 8;
const size_t MAX_N_TASKS = 1 << 8;
const size_t MAX_ARGS    = 1 << 6;

const char * const ERR_ARGS = "ERROR: Please specify file (see --help)\n";
const char * const ERR_FILE = "ERROR: Cannot open specified file\n";

struct Task
{
  char * buffer;
  char **argv;
  char **envp;
  int argc;
  int delay;
};

int interpretFile(const char * const filename, char *envp[]);
int runOnDelay(int delay, char *prog_argv[], char *envp[]);
int skipSpaces(FILE *file);

int main(int argc, char *argv[], char *envp[]) {
  if (argc < 2) {
    fprintf(stderr, ERR_ARGS);
    return 1;
  }

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help")) {
      printf("--help - display this help message\n"
             "--file [-f] <filename> - input file\n");

      return 0;
    } else if (!strcmp(argv[i], "-f") ||
        !strcmp(argv[i], "--file")) {

      if (i == argc) {
        fprintf(stderr, ERR_ARGS);
        return 1;
      }

      int err = interpretFile(argv[i+1], envp);
      if (err) return 1;
    }
  }

  return 0;
}

int interpretFile(const char * const filename, char *envp[]) {
  assert(filename);
  assert(envp);

  FILE *file = fopen(filename, "rb");
  if (!file) {
    fprintf(stderr, ERR_FILE);

    return 2;
  }

  fprintf(stderr, "parent pid = %d\n", getpid());

  Task tasks[MAX_N_TASKS] = {};
  for (size_t i = 0; i < MAX_N_TASKS; i++) {
    tasks[i].buffer = (char *)  calloc(MAX_COMMAND, sizeof(char));
    tasks[i].argv   = (char **) calloc(MAX_ARGS,    sizeof(char *));
  }

  unsigned n_tasks = 0;
  while (1) {
    if (skipSpaces(file)) break;

    fscanf(file, "%d", &tasks[n_tasks].delay);

    // fill the buffer
    int ch = fgetc(file);
    size_t i = 0;
    while (ch != '\n' && ch != EOF && i < MAX_COMMAND) {
      tasks[n_tasks].buffer[i++] = ch;

      ch = fgetc(file);
    }

    // tokenize to argv
    size_t argc = 0;
    char *p2 = strtok(tasks[n_tasks].buffer, " ");
    while (p2 && argc < MAX_ARGS - 1)
      {
        tasks[n_tasks].argv[argc++] = p2;
        p2 = strtok(0, " ");
      }

    tasks[n_tasks].argv[argc] = 0;
    tasks[n_tasks].argc = argc;

    n_tasks++;
  }

  // forking
  for (unsigned i = 0; i < n_tasks; i++)
    if (fork() == 0) exit(runOnDelay(tasks[i].delay, tasks[i].argv, envp));

  // wait for all the processes to finish
  int status = 0;
  while(wait(&status) > 0)  ;

  // check file empty
  if (fgetc(file) != EOF) fprintf(stderr, "WARNING: file not empty at the end\n");

  // cleanup
  for (size_t i = 0; i < MAX_N_TASKS; i++) {
    free(tasks[i].buffer);
    free(tasks[i].argv);
  }

  fclose(file);

  return 0;
}

int runOnDelay(int delay, char *prog_argv[], char *envp[]) {
  assert(delay >= 0);
  assert(prog_argv);

  sleep(delay);

  fprintf(stderr, "====================== [pid = %d] ||| %s + %d sec ======================\n", getpid(), prog_argv[0], delay);

  execve(prog_argv[0], prog_argv, envp);

  return 0;
}

int skipSpaces(FILE *file) {
  int ch = 0;

  while (isspace(ch = fgetc(file))) ;
  if (ch == EOF) return EOF;

  ungetc(ch, file);

  return 0;
}
