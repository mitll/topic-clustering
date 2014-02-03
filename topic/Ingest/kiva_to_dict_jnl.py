#!/usr/bin/env python
#
# Copyright (c) 2013
# Massachusetts Institute of Technology
#
# All Rights Reserved
#

#
# Convert from raw tab delimited to a hash for journal description table
#

# BC, 4/2013

def get_fields (ln):
    output = {}

    f = ln.split('\t')
    num_fields = len(f)
    if (num_fields < 9):
        output = None 
        return output

    output['id'] = f[0]
    output['subject'] = f[1]
    output['msg'] = f[2]
    output['author'] = f[3]
    output['image_id'] = f[4]
    output['image_template_id'] = f[5]
    output['comment_count'] = f[6]
    output['recommendation_count'] = f[7]
    output['date'] = f[8]
    return output
