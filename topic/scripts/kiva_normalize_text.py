#!/usr/bin/env python

#
# Copyright (c) 2013
# Massachusetts Institute of Technology
#
# All Rights Reserved
#

# Normalize message in the Kiva data set
# 

# BC, 3/27/13, 4/11/13

import re

def normalize_msg (xact, debug):
    msg = xact['msg']
    if (debug > 0):
        print u"msg: {}".format(msg)

    # Spurious return, newlines
    msg = msg.replace(u'\\r', ' ').replace(u'\\n',' ')

    # Spurious HTML tags
    msg = re.sub(u'<[a-z].*?>', ' ', msg)
    msg = re.sub(u'</[a-z].*?>', ' ', msg)
    msg = re.sub(u'<[a-z].*?/>', ' ', msg)

    # Extra spaces
    msg = re.sub(u'\s+', ' ', msg)

    xact['msg_norm'] = msg
    if (debug > 0):
        print u"normalized msg: {}".format(msg)
        print
