#include <getopt.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int num_processes = 4;
int num_snapshots = 5;
int seed = 100;
int*** channels;

int max(int a, int b) {
  return a > b ? a : b;
}

// Returns a random int in {0,...,n-1}
int randn(int n) {
  return (int)(((double)rand()) / RAND_MAX * n);
}

// Returns a random process id, excluding the passed in id
int random_process(int id) {
  int result = randn(num_processes - 1);
  if (result == id) {
    result = num_processes - 1;
  }
  return result;
}

void parse_flags(int argc, char** argv) {
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
        seed = atoi(optarg);
        break;
    }
  }
}

typedef enum {
  MONEY_TRANSFER = 1,
} message_type_t;

typedef enum {
  SEND = 0,
  RECV,
} message_dir_t;

typedef struct {
  message_type_t type;
  message_dir_t dir;
  int lamport_timestamp;
  int from;
  int to;

  // data specific to the type of message
  int transfer_amt;
} message_t;

typedef struct {
  int id;
  int money;
  int next_lamport_timestamp;
  size_t message_log_size;
  message_t** message_log;
} process_t;

void process_init(process_t* p, int id) {
  p->id = id;
  p->money = 100;
  p->next_lamport_timestamp = 0;
  p->message_log_size = 0;
  p->message_log = NULL;
}

void process_store_message(process_t* p, message_t* msg) {
  p->message_log = realloc(p->message_log, sizeof(message_t*) * ++p->message_log_size);
  p->message_log[p->message_log_size - 1] = msg;
  printf("Process %d stored a message with timestamp %d\n", p->id, msg->lamport_timestamp);
}

void process_receive_message(process_t* p, int fd, int from) {
  message_t* msg = malloc(sizeof(message_t));
  msg->dir = RECV;
  msg->from = from;
  msg->to = p->id;

  int send_lamport_timestamp;
  if (read(fd, &send_lamport_timestamp, sizeof(send_lamport_timestamp)) != sizeof(send_lamport_timestamp)) {
    perror("read error");
    return;
  }
  msg->lamport_timestamp = max(send_lamport_timestamp, p->next_lamport_timestamp);
  p->next_lamport_timestamp = msg->lamport_timestamp + 1;

  if (read(fd, &msg->type, sizeof(msg->type)) != sizeof(msg->type)) {
    perror("read error");
    return;
  }

  if (msg->type == MONEY_TRANSFER) {
    read(fd, &msg->transfer_amt, sizeof(msg->transfer_amt));
    p->money += msg->transfer_amt;
  } else {
    fprintf(stderr, "Undefined message type id %d\n", msg->type);
  }

  process_store_message(p, msg);
}

void process_send_money(process_t* p, int fd, int to) {
  message_t* msg = malloc(sizeof(message_t));
  msg->lamport_timestamp = p->next_lamport_timestamp++;
  msg->type = MONEY_TRANSFER;
  msg->dir = SEND;
  msg->from = p->id;
  msg->to = to;
  msg->transfer_amt = randn(256);

  p->money -= msg->transfer_amt;

  write(fd, &msg->lamport_timestamp, sizeof(msg->lamport_timestamp));
  write(fd, &msg->type, sizeof(msg->type));
  write(fd, &msg->transfer_amt, sizeof(msg->transfer_amt));

  process_store_message(p, msg);
}

void process_run(process_t* p) {
  srand(seed + p->id);  // so that each process generates unique random numbers

  // leave open only the channels relevant to this process
  int i, j;
  for (i = 0; i < num_processes; ++i) {
    for (j = 0; j < num_processes; ++j) {
      if (i == j) {
        // the channels on the diagonal are useless
        close(channels[i][j][0]);
        close(channels[i][j][1]);
      } else if (i == p->id) {
        // we care only about the sending end, close the receiving end
        close(channels[i][j][1]);
      } else if (j == p->id) {
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
  
  // construct a poll set for reading
  struct pollfd* read_fds = malloc(sizeof(struct pollfd) * num_processes);
  for (i = 0; i < num_processes; ++i) {
    if (i == p->id) {
      read_fds[i].fd = -1;
    } else {
      read_fds[i].fd = channels[i][p->id][1];
      read_fds[i].events = POLLIN;
    }
  }
  struct pollfd* write_fds = malloc(sizeof(struct pollfd) * num_processes);
  for (i = 0; i < num_processes; ++i) {
    if (i == p->id) {
      write_fds[i].fd = -1;
    } else {
      write_fds[i].fd = channels[p->id][i][0];
      write_fds[i].events = POLLOUT;
    }
  }

  while (1) {
    if (randn(5)) {
      poll(read_fds, num_processes, 300);
      for (i = 0; i < num_processes; ++i) {
        if (read_fds[i].revents & POLLIN) {
          process_receive_message(p, read_fds[i].fd, i);
        }
      }
    } else {
      poll(write_fds, num_processes, 300);
      for (i = 0; i < num_processes; ++i) {
        if (write_fds[i].revents & POLLOUT) {
          process_send_money(p, write_fds[i].fd, i);
        }
      }
    }
    printf("process %d: %d money\n", p->id, p->money);
  }
}

/* Our model: we have one "driver" process (the main process) responsible for 
 * spawning the relevant sub-processes and establishing the channels between
 * them. The processes themselves then do the communication and, eg, timestamp
 * assigning.
 */
int main(int argc, char** argv) {
  parse_flags(argc, argv);


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
    if (fork() == 0) {
      process_t p;
      process_init(&p, i);
      process_run(&p);
      exit(0);
    }
  }

  while (waitpid(-1, NULL, 0));

  // TODO: free channels

  return 0;
}
