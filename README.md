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

We create channels between each set of processes. Each process has a 4/5 chance of reading from its poll read set (all processes but itself) and a 1/5 chance of sending a message. 
The two invariants in our program are money and widgets. There are 3 types of messages: marker, widget transfer, and money transfer. On receipt of a message with money/widgets, a 
process will respond to the message with the appropriate amount of widgets/money based on an established exchange rate. On receipt of a marker message, the process will respond
based on its current state with respect to the Chandy & Lamport Snapshot Algorithm. Each process has a 2D array of flags to determine whether it is currently recording incoming messages 
on a channel for a specific snapshot id. If it is, then it will log messages received on that channel to `snapshot.i`.   

## Code formatting

All C code is formatted using `clang-format`'s LLVM style. All python code conforms to the [PEP8](http://legacy.python.org/dev/peps/pep-0008/) standard.
