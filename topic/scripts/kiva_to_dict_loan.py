#!/usr/bin/env python
#
# Copyright (c) 2013
# Massachusetts Institute of Technology
#
# All Rights Reserved
#

#
# Convert from raw tab delimited to a hash
# Could probably generalize with schema file, but do this in a custom way

# BC, 4/2013

def get_fields (ln):
    output = {}
    f = ln.split('\t')
    num_fields = len(f)
    if (num_fields < 9):
        output = None 
        return output
    output['id'] = f[0]
    output['date'] = f[15]
    output['msg'] = f[17]
    return output

