#!/usr/bin/python
import pprint
from collections import defaultdict, namedtuple
from sys import argv, exit

from lib import *


def print_snapshot(snapshot):
    statuses = [s for s in snapshot if isinstance(s, Status)]
    messages = [
        s for s in snapshot if isinstance(
            s,
            WidgetMessage) or isinstance(
            s,
            MoneyMessage)]

    for status in statuses:
        print "Process %d" % status.process
        print "\tLogical: %d" % status.logical
        print "\tVector: %r" % status.vector
        print "\tMoney: %d" % status.money
        print "\tWidgets: %d" % status.widgets

    print

    channels = defaultdict(list)
    for message in messages:
        k = (message.from_id, message.to_id)
        channels[k].append(message)

    for (to_id, from_id) in sorted(channels.keys()):
        print "Channel %d --> %d:" % (to_id, from_id)
        for msg in channels[(to_id, from_id)]:
            print "\tLogical: %d" % msg.logical
            print "\tVector: %r" % msg.vector
            if hasattr(msg, 'money'):
                print "\tMoney: %d" % msg.money
            if hasattr(msg, 'widgets'):
                print "\tWidgets: %d" % msg.widgets
            print


def main():
    if len(argv) < 2:
        print "Usage: %s <snapshot_id>" % argv[0]
        exit(1)
    snapshot_id = int(argv[1])
    snapshots = read_snapshots()
    if snapshot_id in snapshots:
        print_snapshot(snapshots[snapshot_id])
    else:
        print "No snapshot found with id %d" % snapshot_id

if __name__ == '__main__':
    main()
