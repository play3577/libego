#!/usr/bin/python

import sys
import re, os

def toCoords (m):
    rows = "ABCDEFGHJKLMNOPQRST"
    return "%s%s" % (rows [ord(m[0]) - ord('a')], 19-ord(m[1]) + ord('a'))

def doFile (name):
    f = open(name, 'rb').read()
    f = filter(lambda g: not g.isspace(), f)

    assert (f[0] == '(' and f[-1] == ')')
    f = f[1:-1]
    l = f.split (';');

    assert (l[0] == "")

    header = l[1]
    moves = l[2:]

    size = re.search('SZ\\[([^]]*)\\]', header).group(1)
    print size


    handi = re.search('HA\[[1-9]\]AB((\[..\])+)', header)
    if handi != None:
        hcs = handi.group(1).split("[")
        assert (hcs [0] == "")
        hcs = hcs[1:]
        for hc in hcs:
            assert (len(hc) == 3)
            assert (hc[2] == ']')
            print "B", toCoords (hc[:2])


    for m in moves:
        assert (len (m) in (3, 5))
        assert (m[0] in ('W', 'B'))
        assert (m[1] == '[')
        assert (m[-1] == ']')
        if (len(m) == 3): 
            print "%s pass" % (m[0])
        else:
            print "%s %s" % (m[0], toCoords (m[2:4]))
    print


for root, _, files in os.walk(sys.argv[1], topdown=False):
    for name in files:
        doFile ((os.path.join(root, name)))
