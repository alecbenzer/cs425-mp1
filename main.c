#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

void process_flags(int argc, char** argv, int* num_processes,
                   int* num_snapshots) {
  while (1) {
    static struct option long_opts[] = {
      {"num_processes", required_argument, 0, 'p'},
      {"num_snapshots", required_argument, 0, 's'},
      {"seed", required_argument, 0, 'r'},
      {0, 0, 0, 0}
    };

    int option_index = 0;
    int c = getopt_long(argc, argv, "p:s:r:", long_opts, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
      case 'p':
        *num_processes = atoi(optarg);
        break;
      case 's':
        *num_snapshots = atoi(optarg);
        break;
      case 'r':
        srand(atoi(optarg));
    }
  }
}

int main(int argc, char** argv) {
  int num_processes = 10;
  int num_snapshots = 5;

  process_flags(argc, argv, &num_processes, &num_snapshots);

  printf("%d %d\n", num_processes, num_snapshots);
  int i;

  // channels[i][j] is the channel from process i to process j
  int** channels = malloc(sizeof(int*) * num_processes);
  for (i = 0; i < num_processes; ++i) {
    channels[i] = malloc(sizeof(int) * num_processes);
  }

  pid_t* pids = malloc(sizeof(pid_t) * num_processes);
  for (i = 0; i < num_processes; ++i) {
    pids[i] = fork();
    if (pids[i] == 0) {
      // the child
      free(pids);
      printf("A child!\n");
      exit(0);
    } else {
      // the parent
      printf("New child: %d\n", pids[i]);
    }
  }

  free(pids);
  return 0;
}
