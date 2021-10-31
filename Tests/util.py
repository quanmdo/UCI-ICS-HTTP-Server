#!/usr/bin/python3

import sys

if len(sys.argv) is not 3:
    print('./util.py <log_file> <request>')
    sys.exit(1)

with open(sys.argv[1]) as log_file:
    contents = log_file.read().strip()
    if sys.argv[2] in contents:
        sys.exit(0)
    else:
        sys.exit(1)
