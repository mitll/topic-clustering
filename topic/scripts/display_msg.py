#!/usr/bin/env python

#
# Copyright (c) 2013
# Massachusetts Institute of Technology
#
# All Rights Reserved
#

# 
# Display the pickled message file
# 

# BC, 3/27/13

import argparse
import msg_tools as mt
import sys
import codecs

# Main driver: command line interface
if __name__ == '__main__':

    # Parse input command line options
    parser = argparse.ArgumentParser(description="Display all messages with a particular key.")
    parser.add_argument("--input_file", help="input pickled file of messages", type=str, required=True)
    parser.add_argument("--output_file", help="optional text output file", type=str)
    parser.add_argument("--key", help="key of the form key=value, e.g. lid_lui=fr", type=str)
    args = parser.parse_args()
    input_file = args.input_file
    output_file = args.output_file
    key_search = args.key

    if (key_search!=None):
        (ky1, ky2) = key_search.split("=")

    print 'Reading in file: {}'.format(input_file)
    transactions = mt.load_msg(input_file)
    print 'Done'

    if (output_file==None):
        outfile = codecs.getwriter('utf-8')(sys.stdout)
    else:
        outfile = codecs.open(output_file, 'w', encoding='utf-8')

    for key in transactions.keys():
        value = transactions[key]
        if (key_search!=None):
            if (value.has_key(ky1)):
                if (value[ky1] != ky2):
                    continue
            else:
                continue

        for ky in sorted(value.keys()):
            if (type(value[ky])==list):
                outfile.write(u"{} [".format(ky))
                for val in value[ky]:
                   if (type(val)==tuple):
                       outfile.write(u"({},{}) ".format(val[0],val[1]))
                   else:
                       outfile.write(u"{} ".format(val))
                outfile.write(u"\b]\n")
            else:
                outfile.write(u"{} {}\n".format(ky, value[ky]))
        outfile.write(u"\n")

    if (outfile!=None):
        outfile.close()
