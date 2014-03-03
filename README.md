# CS 425 MP1

Code for Alec Benzer and Vignesh Raja's solution to MP1.

## Building & Running

Use the `Makefile` to build the project and then run the `bin/main` binary to run the simulation.

    $ make
    $ bin/main
    $ bin/main --num_processes=10 --num_snapshots=20

The simulation will generate a `log.i` and `snapshot.i` file for each process id `i` in `{0,...,num_processes-1}`.

To verify properties of the output files, run `./test.py`. You can also view the details of a particular snapshot with `./snapshot_util.py <snapshot_id>`.

## Code formatting

All C code is formatted using `clang-format`'s LLVM style. All python code conforms to the [PEP8](http://legacy.python.org/dev/peps/pep-0008/) standard.
