import re
import os
from collections import defaultdict, namedtuple

import vector_timestamp

Status = namedtuple('Status', ['process', 'logical', 'vector', 'money', 'widgets'])
WidgetMessage = namedtuple('WidgetMessage', ['logical', 'vector', 'from_id', 'to_id', 'widgets'])
MoneyMessage = namedtuple('MoneyMessage', ['logical', 'vector', 'from_id', 'to_id', 'money'])

def parse_line(line, pid):
    fields = [s.strip() for s in line.split(':')]

    snapshot_field = fields[0].strip().split() 
    assert len(snapshot_field) == 2
    assert snapshot_field[0] == 'snapshot'
    snapshot = int(snapshot_field[1])

    print fields[1]
    logical_field = fields[1].strip().split()
    assert len(logical_field) == 2
    assert logical_field[0] == 'logical'
    logical = int(logical_field[1])

    vector_field = fields[2].strip().split()
    print vector_field
    assert vector_field[0] == 'vector'
    vector = [int(t) for t in vector_field[1:]]

    if len(fields) == 4:  # it's a process status
        vars_field = fields[3].strip().split()
        assert vars_field[0] == 'money'
        assert vars_field[2] == 'widgets'
        money = int(vars_field[1])
        widgets = int(vars_field[3])

        return Status(pid, logical, vector, money, widgets)
    elif len(fields) == 5:  # it's a message
        from_field = fields[3].strip().split()
        assert from_field[0] == 'from'
        from_id = int(from_field[1])

        message_field = fields[4].strip().split()
        if message_field[0] == 'widgets':
            widgets = int(message_field[1])
            return WidgetMessage(logical, vector, from_id, pid, widgets)
        elif message_field[0] == 'money':
            money = int(message_field[2])
            return MoneyMessage(logical, vector, from_id, pid, money)
        else:
            raise "Parse error"
    else:
        raise "Parse error"

def main():
    p = re.compile("^snapshot.(\\d)+$")
    filenames = [fn for fn in os.listdir('.') if p.match(fn)]

    snapshot = defaultdict(list)  # a dictionary of snapshot ids to lists of events
    for fn in filenames:
        pid = int(p.match(fn).group(1))
        with open(fn, 'r') as f:
            for line in f:
                print parse_line(line, pid)

if __name__ == '__main__':
    main()

