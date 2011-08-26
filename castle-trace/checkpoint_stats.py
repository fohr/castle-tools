#!/usr/bin/env python
import sys
import os
import getopt
import re

class event_trace:
    start_count = []
    end_count   = []
    count_diff  = []
    name  = None
    class state_enum:
        NULL_STATE     = 1
        DATA_ENTRY     = 2
        DATA_ANALYSIS = 3

    state = state_enum.NULL_STATE

    def __init__(self, name):
        self.name=name
        self.mean_count = None
        self.max_count  = None
        self.min_count  = None
        self.state = event_trace.state_enum.DATA_ENTRY

    def add_start_count(self, timestamp):
        if self.state != event_trace.state_enum.DATA_ENTRY:
            raise Exception("not in DATA_ENTRY mode")
        self.start_count.append(timestamp)

    def add_end_count(self, timestamp):
        if self.state != event_trace.state_enum.DATA_ENTRY:
            raise Exception("not in DATA_ENTRY mode")
        self.end_count.append(timestamp)

    def sanity_check(self):
        if self.state != event_trace.state_enum.DATA_ENTRY and self.state != event_trace.state_enum.DATA_ANALYSIS:
            raise Exception("not in DATA_ENTRY or DATA_ANALYSIS mode")

        # check that both lists are same length
        if len(self.start_count) != len(self.end_count):
            return -1

        # check that end_count > start_count
        for x in range(0, len(self.start_count)):
            if self.end_count[x] < self.start_count[x]:
                print self.start_count[x], " ... ", self.end_count[x]
                return -2

        # check for overlapping events
        if len(self.end_count)>1:
            for x in range(1, len(self.end_count)):
                if self.start_count[x-1] > self.end_count[x]:
                    return -3

        return 0

    def return_stats(self):
        if self.state != event_trace.state_enum.DATA_ANALYSIS:
            raise Exception("not in DATA_ANALYSIS mode")
        ret_dict = {}
        ret_dict["mean"] = self.mean_count
        ret_dict["max"]  = self.max_count
        ret_dict["min"]  = self.min_count
        return ret_dict

    def analyse(self):
        if self.state != event_trace.state_enum.DATA_ENTRY:
            raise Exception("not in DATA_ENTRY mode")
        ret = self.sanity_check()
        if ret != 0:
            raise Exception("Failed sanity check, error code "+str(ret))
        self.state = event_trace.state_enum.DATA_ANALYSIS
        event_diff_total = 0
        count =0
        for x in range(0, len(self.start_count)):
            event_diff = self.end_count[x] - self.start_count[x]
            event_diff_total += event_diff
            count += 1
            if x==0:
                self.max_count = event_diff
                self.min_count = event_diff
            else:
                self.max_count = max(self.max_count, event_diff)
                self.min_count = min(self.min_count, event_diff)
        self.mean_count=event_diff_total / count
        return self.return_stats()

def main(argv=None):

    if argv is None:
        argv=sys.argv
    if len(argv) != 2:
        raise Exception("must specify argument: tracefile name")
    if not os.path.exists(sys.argv[1]):
        raise Exception("File "+argv[1]+" not found")

    tracefile=open(argv[1], 'r')
    checkpoint_stats = event_trace("checkpoint")

    for line in tracefile:
        if re.search('checkpoint', line.lower()):
            line_segment    = line.lower().split(',')
            timestamp_secs  = int(re.sub('[\D]', '', line_segment[1]))
            timestamp_usecs = int(re.sub('[\D]', '', line_segment[2]))
            timestamp=timestamp_secs+(timestamp_usecs/1E6)
            if re.search('start', line_segment[0]):
                checkpoint_stats.add_start_count(timestamp)
            elif re.search('end', line_segment[0]):
                checkpoint_stats.add_end_count(timestamp)

    tracefile.close()
    print checkpoint_stats.analyse()

if __name__ == '__main__': 
    main()
