#include <getopt.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach.h>
#endif
int num_processes = 4;
int num_snapshots = 5;
int seed = 100;
int ***channels;
int exchange_rate = 20; //$20 per widget

// Portable (OSX, Linux) function to get system time with nanosecond percision.
void get_time(struct timespec *ts) {
#ifdef __APPLE__
  clock_serv_t cclock;
  mach_timespec_t mts;
  host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
  clock_get_time(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);
  ts->tv_sec = mts.tv_sec;
  ts->tv_nsec = mts.tv_nsec;

#else
  clock_gettime(CLOCK_REALTIME, ts);
#endif
}

int max(int a, int b) { return a > b ? a : b; }

void print_vector_timestamp_spaced(FILE *f, int *timestamp) {
  int i;
  for (i = 0; i < num_processes; i++) {
    fprintf(f, "%i", timestamp[i]);
    if (i != num_processes - 1)
      fprintf(f, " ");
  }
}

void print_vector_timestamp_brackets(FILE *f, int *timestamp) {
  int i;
  fprintf(f, "[");
  for (i = 0; i < num_processes; i++) {
    fprintf(f, "%i", timestamp[i]);
    if (i != num_processes - 1)
      fprintf(f, ",");
  }
  fprintf(f, "]");
}

// Returns a random int in {0,...,n-1}
int randint(int n) { return (int)(((double)rand()) / RAND_MAX * n); }

// Returns a random process id, excluding the passed in id
int random_process(int id) {
  int result = randint(num_processes - 1);
  if (result == id) {
    result = num_processes - 1;
  }
  return result;
}

void parse_flags(int argc, char **argv) {
  while (1) {
    static struct option long_opts[] = {
      { "num_processes", required_argument, 0, 'p' },
      { "num_snapshots", required_argument, 0, 's' },
      { "seed", required_argument, 0, 'r' },
      { 0, 0, 0, 0 }
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
  WIDGET_TRANSFER = 0,
  MONEY_TRANSFER = 1,
  MARKER = 2,
} message_type_t;

typedef enum {
  SEND = 0,
  RECV = 1,
} message_dir_t;

typedef struct {
  message_type_t type;
  message_dir_t dir;
  int lamport_timestamp;
  int *vector_timestamp;
  struct timespec real_timestamp;
  int from;
  int to;

  // data specific to the type of message
  int transfer_amt;
  int response_requested;

  int snapshot_id;
} message_t;

typedef struct {
  int id;
  int money;
  int widgets;
  int next_lamport_timestamp;
  int *next_vector_timestamp;
  size_t message_log_size;
  message_t **message_log;
  FILE *log_file;
  FILE *snapshot_file;

  // used only by process 0
  int snapshot_count;

  // received_marker[i] indicates that on snapshot i, the process has already
  // received a marker
  int *received_marker;
  // recording[i][j] indicates whether we're recoding incoming channel i for
  // snapshot j
  int **recording;
} process_t;

bool marker_received(process_t *p, int snapshot_num) {
  return p->received_marker[snapshot_num];
}

void process_init(process_t *p, int id) {
  p->id = id;
  p->money = 100;
  p->widgets = 20;
  p->next_lamport_timestamp = 0;
  p->next_vector_timestamp = (int *)malloc(num_processes * sizeof(int));
  p->message_log_size = 0;
  p->message_log = NULL;

  char snapshot_file_name[1024];
  snprintf(snapshot_file_name, sizeof(snapshot_file_name), "snapshot.%d", id);
  p->snapshot_file = fopen(snapshot_file_name, "w");

  char log_file_name[1024];
  snprintf(log_file_name, sizeof(log_file_name), "log.%d", id);
  p->log_file = fopen(log_file_name, "w");
  fprintf(p->log_file, "# from lamport vector real\n");

  p->snapshot_count = 0;

  p->received_marker = (int *)malloc(sizeof(int) * num_snapshots);
  int j;
  for (j = 0; j < num_snapshots; ++j) {
    p->received_marker[j] = 0;
  }

  p->recording = malloc(sizeof(int *) * num_processes);
  int i;
  for (i = 0; i < num_processes; ++i) {
    p->recording[i] = malloc(sizeof(int) * num_snapshots);
    int k;
    for (k = 0; k < num_snapshots; ++k) {
      p->recording[i][k] = 0;
    }
  }
}

void process_store_message(process_t *p, message_t *msg) {
  p->message_log =
      realloc(p->message_log, sizeof(message_t *) * ++p->message_log_size);
  p->message_log[p->message_log_size - 1] = msg;
  fprintf(p->log_file, "%d %d ", msg->from, msg->lamport_timestamp);
  print_vector_timestamp_brackets(p->log_file, msg->vector_timestamp);
  fprintf(p->log_file, " %lld.%.9ld\n", (long long)msg->real_timestamp.tv_sec,
          msg->real_timestamp.tv_nsec);
  fflush(p->log_file);

  // check if we should write to a snapshot file
  int snapshot_id;
  for (snapshot_id = 0; snapshot_id < num_snapshots; ++snapshot_id) {
    if (p->recording[msg->from][snapshot_id]) {
      if (msg->type != MARKER) {
        fprintf(p->snapshot_file, "snapshot %d : logical %d : ", snapshot_id,
                p->next_lamport_timestamp);
        fprintf(p->snapshot_file, "vector ");
        print_vector_timestamp_spaced(p->snapshot_file, p->next_vector_timestamp);
        if (msg->type == MONEY_TRANSFER)
          fprintf(p->snapshot_file, " : from %d : money %d\n", msg->from,
                  msg->transfer_amt);
        else if (msg->type == WIDGET_TRANSFER)
          fprintf(p->snapshot_file, " : from %d : widgets %d\n", msg->from,
                  msg->transfer_amt);
        fflush(p->snapshot_file);
      }
    }
  }
}

void message_init(message_t *msg, process_t *p) {
  get_time(&msg->real_timestamp);
  msg->lamport_timestamp = p->next_lamport_timestamp++;
  p->next_vector_timestamp[p->id]++;
  msg->vector_timestamp = p->next_vector_timestamp;
  msg->dir = SEND;
}

void send_message_header(message_t *msg, int fd) {
  write(fd, &msg->lamport_timestamp, sizeof(msg->lamport_timestamp));
  write(fd, msg->vector_timestamp, sizeof(int) * num_processes);
  write(fd, &msg->type, sizeof(msg->type));
}

void read_message_header(message_t *msg, process_t *p, int fd) {
  int lamport_timestamp;
  read(fd, &lamport_timestamp, sizeof(lamport_timestamp));
  msg->lamport_timestamp =
      max(lamport_timestamp, p->next_lamport_timestamp) + 1;
  p->next_lamport_timestamp = msg->lamport_timestamp + 1;

  msg->vector_timestamp = (int *)malloc(num_processes * sizeof(int));
  int j;
  for (j = 0; j < num_processes; j++) {
    msg->vector_timestamp[j] = 0;
  }
  int *send_vector_timestamp = (int *)malloc(num_processes * sizeof(int));
  if (read(fd, send_vector_timestamp, sizeof(int) * num_processes) !=
      sizeof(int) * num_processes) {
    perror("read error");
    return;
  }
  // compute vector timestamp based on received timestamp
  int i;
  for (i = 0; i < num_processes; ++i) {
    if (i == p->id) {
      continue;
    }
    msg->vector_timestamp[i] =
        max(send_vector_timestamp[i], (p->next_vector_timestamp)[i]);
  }
  p->next_vector_timestamp[p->id]++;
  msg->vector_timestamp[p->id] = p->next_vector_timestamp[p->id];
  p->next_vector_timestamp = msg->vector_timestamp;

  if (read(fd, &msg->type, sizeof(msg->type)) != sizeof(msg->type)) {
    perror("read error");
    return;
  }
  get_time(&msg->real_timestamp);
}

/*
 * response_requested indicates whether the currency is being sent in response
 * to a transaction
 * or to initiate it amt is the amount of currency you want to send. if set to
 * -1, a random currency is sent, o/w the specified amt is sent
 */
void process_send_currency(process_t *p, int fd, int to, int type,
                           int response_requested, int amt) {
  message_t *msg = malloc(sizeof(message_t));
  message_init(msg, p);

  msg->from = p->id;
  msg->to = to;
  msg->response_requested = response_requested;

  bool amt_defined = (amt != -1);

  if (type) {
    msg->type = MONEY_TRANSFER;
    // check whether an amount is specified
    if (!amt_defined)
      msg->transfer_amt = randint(256);
    else
      msg->transfer_amt = amt;
    p->money -= msg->transfer_amt;
  } else {
    msg->type = WIDGET_TRANSFER;
    if (!amt_defined)
      msg->transfer_amt = randint(21);
    else
      msg->transfer_amt = amt;
    p->widgets -= msg->transfer_amt;
  }

  process_store_message(p, msg);

  send_message_header(msg, fd);
  write(fd, &msg->response_requested, sizeof(msg->response_requested));
  write(fd, &msg->transfer_amt, sizeof(msg->transfer_amt));
}

void send_markers(process_t *p, int snapshot_id) {
  int i;
  for (i = 0; i < num_processes; ++i) {
    if (i == p->id) {
      continue;
    }
    message_t *msg = malloc(sizeof(message_t));
    message_init(msg, p);
    msg->type = MARKER;
    msg->snapshot_id = snapshot_id;
    send_message_header(msg, channels[p->id][i][0]);
    write(channels[p->id][i][0], &msg->snapshot_id, sizeof(msg->snapshot_id));
  }
}

void record_process_state(process_t *p, int snapshot_id) {
  fprintf(p->snapshot_file, "snapshot %d : logical %d : ", snapshot_id,
          p->next_lamport_timestamp);
  fprintf(p->snapshot_file, "vector ");
  print_vector_timestamp_spaced(p->snapshot_file, p->next_vector_timestamp);
  fprintf(p->snapshot_file, " : money %d widgets %d\n", p->money, p->widgets);
  fflush(p->snapshot_file);
}

bool done_with_snapshot(process_t* p, int snapshot_id) {
  int i, j;
  for (i = 0; i < num_processes; ++i) {
    if (p->recording[i][snapshot_id] == 1) {
      return false;
    }
  }
  return true;
}

void process_receive_message(process_t *p, int fd, int from) {
  message_t *msg = malloc(sizeof(message_t));

  read_message_header(msg, p, fd);
  msg->dir = RECV;
  msg->from = from;
  msg->to = p->id;


  if (msg->type == MARKER) {
    process_store_message(p, msg);
    int snapshot_id;
    read(fd, &snapshot_id, sizeof(snapshot_id));
    if (!marker_received(p, snapshot_id)) {
      // record state of process
      record_process_state(p, snapshot_id);
      p->received_marker[snapshot_id] = 1;
      int i;
      // start recording on all channels
      for (i = 0; i < num_processes; ++i) {
        if (i == p->id || i == from) {
          continue;
        }
        if (!p->recording[i][snapshot_id])
          p->recording[i][snapshot_id] = 1;
      }
      // send out markers to all other processes
      send_markers(p, snapshot_id);
    } else {
      // stop recording on the channel
      p->recording[from][snapshot_id] = 0;
      if (done_with_snapshot(p, snapshot_id)) {
        printf("Process %d done with snapshot %d\n", p->id, snapshot_id);
      }
    }

  } else {
    if (read(fd, &msg->response_requested, sizeof(msg->response_requested)) !=
        sizeof(msg->response_requested)) {
      perror("read error");
      return;
    }
    if (msg->type == MONEY_TRANSFER) {
      read(fd, &msg->transfer_amt, sizeof(msg->transfer_amt));
      p->money += msg->transfer_amt;
      process_store_message(p, msg);
      if (msg->response_requested) {
        int sendback_amt = msg->transfer_amt / exchange_rate;
        // need to send back widgets, received money
        process_send_currency(p, channels[p->id][msg->from][0], msg->from,
                              WIDGET_TRANSFER, 0, sendback_amt);
      }
    } else if (msg->type == WIDGET_TRANSFER) {
      read(fd, &msg->transfer_amt, sizeof(msg->transfer_amt));
      p->widgets += msg->transfer_amt;
      process_store_message(p, msg);
      if (msg->response_requested) {
        int sendback_amt = msg->transfer_amt * exchange_rate;
        // need to send back money, received widgets
        process_send_currency(p, channels[p->id][msg->from][0], msg->from,
                              MONEY_TRANSFER, 0, sendback_amt);
      }
    } else {
      process_store_message(p, msg);
      fprintf(stderr, "Undefined message type id %d from process %d\n",
              msg->type, from);
    }
  }
}

// Will only every be called with p being process 0
void initiate_snapshot(process_t *p) {
  int snapshot_id = p->snapshot_count++;
  printf("Initiating snapshot %d\n", snapshot_id);
  record_process_state(p, snapshot_id);
  send_markers(p, snapshot_id);
  p->received_marker[snapshot_id] = 1;
  // turn on recording on all channels
  int i;
  for (i = 0; i < num_processes; ++i) {
    if (i == p->id) {
      continue;
    }
    p->recording[i][snapshot_id] = 1;
  }
}

void process_run(process_t *p) {
  srand(seed + p->id); // so that each process generates unique random numbers

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
  struct pollfd *read_fds = malloc(sizeof(struct pollfd) * num_processes);
  for (i = 0; i < num_processes; ++i) {
    if (i == p->id) {
      read_fds[i].fd = -1;
    } else {
      read_fds[i].fd = channels[i][p->id][1];
      read_fds[i].events = POLLIN;
    }
  }
  struct pollfd *write_fds = malloc(sizeof(struct pollfd) * num_processes);
  for (i = 0; i < num_processes; ++i) {
    if (i == p->id) {
      write_fds[i].fd = -1;
    } else {
      write_fds[i].fd = channels[p->id][i][0];
      write_fds[i].events = POLLOUT;
    }
  }

  while (1) {
    if (randint(5)) {
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
          // randomly generate what type of currency we want to send (widgets or
          // money)
          int currency = randint(2);
          if (currency) {
            process_send_currency(p, write_fds[i].fd, i, MONEY_TRANSFER, 1, -1);

          } else {
            process_send_currency(p, write_fds[i].fd, i, WIDGET_TRANSFER, 1,
                                  -1);

          }
        }
      }
    }

    if (p->id == 0 && p->snapshot_count < num_snapshots) {
      if (!randint(100)) { // 1 in 100
        initiate_snapshot(p);
      }
    }
  }
}

/* Our model: we have one "driver" process (the main process) responsible for
 * spawning the relevant sub-processes and establishing the channels between
 * them. The processes themselves then do the communication and, eg, timestamp
 * assigning.
 */
int main(int argc, char **argv) {
  parse_flags(argc, argv);

  int i, j; // loop indicies

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
  channels = malloc(sizeof(int **) * num_processes);
  for (i = 0; i < num_processes; ++i) {
    channels[i] = malloc(sizeof(int *) * num_processes);
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

  while (waitpid(-1, NULL, 0))
    ;

  // TODO: free channels

  return 0;
}
