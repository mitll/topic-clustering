#!/usr/bin/env python

#
# Copyright (c) 2013
# Massachusetts Institute of Technology
#
# All Rights Reserved
#

# Normalize message for topic id in the Kiva data set
# 

# BC, 4/19/13

import re
import msg_tools as mt

def normalize_msg (xact, rewrite_hash, debug):
    msg = xact['msg_norm']
    if (debug > 0):
        print u"msg: {}".format(msg)

    # Various normalization routines -- pick and choose as needed
    msg = mt.convertUTF8_to_ascii(msg, rewrite_hash)
    msg = mt.remove_markup(msg)
    msg = mt.remove_numbers(msg)
    msg = mt.remove_nonsentential_punctuation(msg)
    msg = mt.remove_capitalized_words(msg)
    msg = mt.remove_isolated_symbols(msg)
    msg = re.sub('\s+',' ', msg)
    if (msg == ' '):
        msg = ''

    xact['msg_topic'] = msg
    if (debug > 0):
        print u"\ntopic normalized msg: {}".format(msg)
        print
        print
