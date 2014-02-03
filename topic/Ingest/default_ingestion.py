#!/usr/bin/env python
#
# Copyright (c) 2013
# Massachusetts Institute of Technology
#
# All Rights Reserved
#

#
# Convert from raw tab delimited to a dictionary. This function assumes that there are two fields
# in the following order: document id and text 
#

# JAA, 4/2013

def get_fields (ln):
    output = {}

    f = ln.split('\t')
    num_fields = len(f)
    if (num_fields < 2):
        return None

    output['id'] = f[0]
    output['msg'] = f[1]
    
    return output
