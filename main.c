#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>

int num_processes = 4;
int num_snapshots = 5;
int*** channels;

// Returns a random int in {0,...,n-1}
int randn(int n) {
  return rand() * n / RAND_MAX;
}

void process_flags(int argc, char** argv) {
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
        num_processes = atoi(optarg);
        break;
      case 's':
        num_snapshots = atoi(optarg);
        break;
      case 'r':
        srand(atoi(optarg));
    }
  }
}

void process(int id) {
  // leave open only the channels relevant to this process
  int i, j;
  for (i = 0; i < num_processes; ++i) {
    for (j = 0; j < num_processes; ++j) {
      if (i == j) {
        // the channels on the diagonal are useless
        close(channels[i][j][0]);
        close(channels[i][j][1]);
      } else if (i == id) {
        // we care only about the sending end, close the receiving end
        close(channels[i][j][1]);
      } else if (j == id) {
        // we care only about the receiving end, close the sending end
        close(channels[i][j][0]);
      } else {
        // this channel has nothing to do with us
        close(channels[i][j][0]);
        close(channels[i][j][1]);
      }
    }
  }

  // we will send messages out on channels[id][*][0] and receive messages on
  // channels[*][id][1]

  // send a message to each process
  for (i = 0; i < num_processes; ++i) {
    if (i == id) {
      continue;
    }
    char* msg;
    int size = asprintf(&msg, "Hello from process %d to process %d", id, i);

    write(channels[id][i][0], msg, size);

    free(msg);
  }

  // receive a message from each process
  for (i = 0; i < num_processes; ++i) {
    if (i == id) {
      continue;
    }
    char buf[1024];
    int n = read(channels[i][id][1], buf, sizeof(buf));
    printf("Process %d got message: '%.*s'\n", id, n, buf);
  }
}

/* Our model: we have one "driver" process (the main process) responsible for 
 * spawning the relevant sub-processes and establishing the channels between
 * them. The processes themselves then do the communication and, eg, timestamp
 * assigning.
 */
int main(int argc, char** argv) {
  process_flags(argc, argv);

  int i, j;  // loop indicies

  /* 
   * channels[i][j] is the channel from process i to process j, with
   * channels[i][j][0] being the i's end and channels[i][j][1] being j's end.
   *
   * Ie, if process i wishes to talk to process j, it will send a message on
   * channels[i][j][0], and if process j wishes to receive such a message from
   * process i, it will read from channels[i][j][1].
   *
   * channels[i][i][0] and channels[i][i][1], for each i, is, of course,
   * wasted, for the sake of a simple indexing scheme.
   */
  channels = malloc(sizeof(int**) * num_processes);
  for (i = 0; i < num_processes; ++i) {
    channels[i] = malloc(sizeof(int*) * num_processes);
    for (j = 0; j < num_processes; ++j) {
      channels[i][j] = malloc(sizeof(int) * 2);
      socketpair(PF_LOCAL, SOCK_STREAM, 0, channels[i][j]);
    }
  }

  for (i = 0; i < num_processes; ++i) {
    pid_t pid = fork();
    if (pid == 0) {
      // In the child, close() all the unneeded sockets
      process(i);
      exit(0);
    } else {
      // the parent
      printf("New child: %d\n", pid);
    }
  }

  // TODO: free channels

  return 0;
}
