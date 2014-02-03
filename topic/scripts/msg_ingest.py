#!/usr/bin/env python
#
# Copyright (c) 2013
# Massachusetts Institute of Technology
#
# All Rights Reserved
#

#
# Ingest data from raw format and serialize
#

# BC 4/2013

import argparse
import os
import msg_tools as mt
import kiva_to_dict_loan as raw_to_dict
import codecs

def read_raw_file (input_file, get_fields, debug):
    transactions = {}
    print 'Reading in file: {}'.format(input_file)
    infile = codecs.open(input_file, encoding='utf-8')
    count = 0
    success = 0
    for ln in infile:
        if ((count % 100000)==0):
            print "\ton line: {}".format(count)
        count += 1
        ln = ln.rstrip()
        xact = get_fields(ln)
        if (xact == None):
            continue
        success += 1
        transactions[xact['id']] = xact
        if (debug>0 and success==2000):
            break
    infile.close()
    print "Percentage of data kept: {} %".format(100.0*((0.0 + success)/count))
    return transactions

# Main driver: command line interface
if __name__ == '__main__':

    # Parse input command line options
    parser = argparse.ArgumentParser(description="Raw file to dictionary (hash).")
    parser.add_argument("--input_file", help="input raw csv table", type=str, required=True)
    parser.add_argument("--output_file", help="output structured file (pickled)", type=str, required=True)
    parser.add_argument("--verbose", help="verbosity > 0, debug mode", type=int, default=0)
    args = parser.parse_args()
    input_file = args.input_file
    output_file = args.output_file
    debug = args.verbose

    transactions = read_raw_file(input_file, raw_to_dict.get_fields, debug)
    mt.save_msg(transactions, output_file)
