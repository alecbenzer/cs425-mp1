# CS 425 MP1

Code for Alec Benzer and Vignesh Raja's solution to MP1.

## Building & Running

Use the `Makefile` to build the project and then run the `bin/main` binary to run the simulation.

    $ make
    $ bin/main
    $ bin/main --num_processes=10 --num_snapshots=20

The simulation will generate a `log.i` and `snapshot.i` file for each process id `i` in `{0,...,num_processes-1}`.

To verify properties of the output files, run `./test.py`. You can also view the details of a particular snapshot with `./snapshot_util.py <snapshot_id>`.

## Description of Algorithm

We create channels between each pair of processes via the `socketpair` system call. In each iteration of a process's execution loop, it has a 4/5 chance of attempting to read a message from an incoming channel and a 1/5 chance of attempting to send out a message.

The two invariants in our program are the amount of money and number of widgets in the system. Each process starts with 100 money and 20 widgets, and thus the total amount of money in the system at any time is `num_processes` * 100, and the total number of widgets is `num_processes` * 20.

There are 3 types of messages: marker messages for the snapshot algorithm, widget transfer messages, and money transfer messages.

On receipt of a money/widget transfer message that a process didn't initiate itself, said process will respond to the message with an appropriate amount of widgets/money based on an established exchange rate.

On receipt of a marker message, the process will respond based on its current state with respect to the Chandy & Lamport Snapshot Algorithm. Each process has a 2D array of flags to determine whether it is currently recording incoming messages on a channel for a specific snapshot id. If it is, then it will log messages received on that channel to `snapshot.i`.   

## Code formatting

All C code is formatted using `clang-format`'s LLVM style. All python code conforms to the [PEP8](http://legacy.python.org/dev/peps/pep-0008/) standard.
