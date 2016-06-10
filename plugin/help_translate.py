#!/usr/bin/env python3.5
"""
Take a document on stdin and format it into an array of C string lines.
"""

_c_literal_trans = str.maketrans({
    '\\': '\\\\',
    '"': '\\"',
})


def format(f):
    lines = ['  "%s"' % l.rstrip().translate(_c_literal_trans) for l in f]
    output = ',\n'.join(lines)
    print(output, end='')


if __name__ == '__main__':
    import sys
    format(sys.stdin)
