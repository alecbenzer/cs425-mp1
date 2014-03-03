#!/usr/bin/python
import numpy as np
from itertools import product

from mp1_lib import *

Message = namedtuple(
    'Message',
    ['from_id',
     'to_id',
     'lamport',
     'vector',
     'real'])


def read_message_logs():
    p = re.compile("^log.(\\d)+$")
    filenames = [fn for fn in os.listdir('.') if p.match(fn)]

    messages = []
    messages_by_process = {}

    for fn in filenames:
        to_id = int(p.match(fn).group(1))
        messages_by_process[to_id] = []
        with open(fn, 'r') as f:
            for line in f:
                if line[0] == '#':
                    continue

                from_id, lamport, vector, real = line.split(' ')

                from_id = int(from_id)
                lamport = int(lamport)
                vector = VectorTimestamp([
                    int(t) for t in vector.strip('[]').split(',')])
                real = np.datetime64(int(float(real) * 1e9), 'ns')

                m = Message(from_id, to_id, lamport, vector, real)
                messages.append(m)
                messages_by_process[to_id].append(m)
    return messages, messages_by_process


def check_invariants(snapshot):
    money = sum(m.money for m in snapshot if hasattr(m, 'money'))
    widgets = sum(m.widgets for m in snapshot if hasattr(m, 'widgets'))
    print "\tmoney: %d, widgets: %d" % (money, widgets)


def main():
    snapshots = read_snapshots()

    for k, snapshot in snapshots.iteritems():
        print "Checking snapshot %d" % k
        check_invariants(snapshot)

    print "Reading message logs..."
    messages, messages_by_process = read_message_logs()

    print "Checking message logs..."
    # Check that each process's timestamps are monotonic
    for msgs in messages_by_process.values():
        for i in range(len(msgs) - 1):
            if msgs[i].real > msgs[i + 1].real:
                print "ERROR"
            if msgs[i].lamport > msgs[i + 1].lamport:
                print "ERROR"
            if not (msgs[i].vector <= msgs[i + 1].vector):
                print "%r should be <= %r" % (msgs[i].vector,
                                              msgs[i + 1].vector)

    print "More message log testing (this may take some time)..."
    # Slow O(n^2) loop, but simpler than trying to do a partial sort or
    # something like that
    for m, n in product(messages, repeat=2):
        if m.vector <= n.vector:
            if not (m.real <= n.real):
                print "%r <= %r but not (%r <= %r)" % (
                    m.vector, n.vector, m.real, n.real)
                if m.vector < n.vector:
                    if not (m.real < n.real):
                        print "%r < %r but not (%r < %r)" % (
                            m.vector, n.vector, m.real, n.real)
                        if not (m.lamport < n.lamport):
                            print "%r < %r but not (%r < %r)" % (
                                m.vector, n.vector, m.lamport, n.lamport)

if __name__ == '__main__':
    main()
