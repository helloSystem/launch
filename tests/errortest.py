#!/usr/bin/env python3

import sys

for i in range(50):
    print("error output", i, file=sys.stderr)

sys.exit(1)