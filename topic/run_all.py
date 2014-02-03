#!/usr/bin/env python
#
# Copyright (c) 2013
# Massachusetts Institute of Technology
#
# All Rights Reserved
#

# 
# Ingest messages from a CSV file, perform normalization, LID, and topic recognition
# 

# Originally written by BC, 4/2013
# BC, revised 5/26-5/30/2013 for release

# Imports
import os
import sys
import scripts.msg_ingest as mi
import scripts.msg_tools as mt
import importlib

# Data specific imports
import scripts.kiva_normalize_text as text_norm
import scripts.kiva_normalize_topic as topic_norm

#Constants
#Keys to argument table: input file
INPUT_FILE_KEY = 'input_file'
CUSTOM_READER_KEY = '-r'
OUTPUT_DIR_KEY = 'output_dir'
DO_SIMPLE_METADATA_KEY = '-do_simple_metadata'
TEMP_DIR_KEY = '-temp'
NUM_TOPICS_KEY = '-num_topics'
STOP_LIST_KEY = '-stop_list'
TF_CUTOFF_KEY = '-tf_cutoff'
DF_CUTOFF_KEY = '-df_cutoff'
DO_TOPIC = '-do_topic'
LANG_FILTER_KEY = '-lang_filter'

#Default reader to ingest raw data
DEFAULT_DATA_READER = 'Ingest.default_ingestion'
SIMPLE_META_EXTENSION = '.simple.pckl'
BASIC_EXTENSION = '.basic.pckl'

# Input variables
dir_topic = 'topic'
do_ingest = True
debug = 0


#Utility functions
def print_usage():
    print
    print('Usage: run_all.py [OPTIONS] INPUT_FILE OUTPUT_DIR')
    print
    print('INPUT_FILE           input file (see format in documentation)')
    print('OUTPUT_DIR           output directory')
    print('-r                   (string, default: "Default_ingestion")') 
    print('                     Custom reader for the ingestion phase')
    print('-do_simple_metadata  (boolean: True|False, default: True)')
    print('                     Whether the simple metadata extraction step is performed or not')
    print('-do_topic            (boolean: True|False, default: True)')
    print('                     Whether the topic clustering step is performed or not')
    print('-temp                (string, default: temp/)')
    print('                     storage location for temp files')
    print('-num_topics          (int, default: 50)')
    print('                     number of topics used for clustering')
    print('-stop_list           (string, default:topic/data/stop_list_kiva.txt)')
    print('                     Terms to exclude from topic clustering') 
    print('-tf_cutoff           (int, default: 3)') 
    print('                     Exclude terms that occur this number of times or fewer')
    print('-df_cutoff           (float, default: 0.25)') 
    print('                     Exclude terms that happen in greater than this fraction of vectors')
    print('-lang_filter         (string, default: "en")')
    print('                     Process only those documents in the specified language for purposes of topic clustering.')
    return

def process_arguments():
    #Process arguments
#    input_file           INPUT_FILE (see format in documentation)
#    output_dir           OUTPUT_DIR (see format of output files and which ones are generated)
#    -r                   (string, default: Default_ingestion') Custom reader for the ingestion phase
#    -do_simple_metadata  (boolean: True|False, default: True) whether the simple metadata extraction step is performed or not
#    -temp                (string, default: temp/) storage location for temp files
#    -num_topics          (int, default: 50)    number of topics used for clustering
#    -stop_list           (string, default:topic/data/stop_list_kiva.txt) Terms to exclude from topic clustering 
#    -tf_cutoff           (int, default: 3) Exclude terms that occur this number of times or fewer
#    -df_cutoff           (float, default: 0.25) Exclude terms that happen in greater than this fraction of vectors
#    -do_topic            (boolean: True|False, default: True)
#    -lang_filter         (string, default:'en') Process only those documents in the specified language for purposes of topic clustering.

    valid_args = frozenset([CUSTOM_READER_KEY, DO_SIMPLE_METADATA_KEY, TEMP_DIR_KEY, NUM_TOPICS_KEY, STOP_LIST_KEY, TF_CUTOFF_KEY, DF_CUTOFF_KEY, DO_TOPIC, LANG_FILTER_KEY]);
    #Extract args
    args = sys.argv[1:]
    #if les than 2 arguments specified print usage and exit
    args_len = len(args)
    if args_len < 2 or args[0]=='h':
        print_usage()
        sys.exit()
    #Extract input file
    arg_table = {}
    arg_table[INPUT_FILE_KEY] = args[args_len - 2]
    arg_table[OUTPUT_DIR_KEY] = args[args_len - 1]
    args = args[:args_len-2]
    for arg_i in range(0,len(args),2):
        a_key = args[arg_i]
        #If argument is invalid, prompt the user and exit
        if a_key not in valid_args:
            print('ERROR: Invalid argument "{}"'.format(a_key)) 
            sys.exit()
        
        try:
            a_value = args[arg_i + 1]
            if (a_key == DO_SIMPLE_METADATA_KEY) or (a_key == DO_TOPIC): #Process boolean args 
                if str(a_value).lower() == 'true':
                    a_value = True
                elif str(a_value).lower() == 'false':
                    a_value = False
                else:
                    print('ERROR: boolean argument requires either "True" or "False" as value. Arg: {}'.format(a_key))
                    sys.exit()
            arg_table[a_key] = a_value
        except IndexError:
            print('ERROR: Argument without value: "{}"'.format(a_key))
            sys.exit()
    return arg_table
        
                
################# MAIN PROCESSING ############################################################### 
#Process arguments
arg_table = process_arguments()
######### DEBUGGING ##########
if(debug > 0):
    print("Arguments:")
    print arg_table
##############################
#Extract input file and check its existence
input_file = arg_table[INPUT_FILE_KEY].strip()
if not os.path.exists(input_file):
    print('ERROR: File "{}" does not exist.'.format(input_file))
    sys.exit()
    
if not os.path.isfile(input_file):
    print('ERROR: "{}" is not a file'.format(input_file) )
    sys.exit()   
# Extract basename to prefix subsequent output files
fn_table = os.path.basename(input_file)

#Validate output dir
output_dir = arg_table[OUTPUT_DIR_KEY].strip()
if not os.path.exists(output_dir):
    print('ERROR: Directory {} does not exist.'.format(output_dir))
    sys.exit()
    
if not os.path.isdir(output_dir):
    print('ERROR: {} is not a directory.'.format(output_dir))
    sys.exit()  
# Make sure it ends in / for future path concatenation
if not output_dir.endswith('/'):
    output_dir += '/'
# Extract args for later use     
do_simple_metadata = arg_table.get(DO_SIMPLE_METADATA_KEY, True)
do_topic = arg_table.get(DO_TOPIC, True)
dir_temp = arg_table.get(TEMP_DIR_KEY,'tmp/')

# Basic ingest: raw to structured form
# If input file is a text file (raw data), read the raw input file, using custom or default reader, into a dictionary and dump the dictionary into a 
# pickle file. Otherwise skip this step.
data_table = None
data_loaded = False

if (input_file.endswith(BASIC_EXTENSION) or input_file.endswith(SIMPLE_META_EXTENSION)):
    do_ingest = False

if (do_ingest):
    print('Reading data...')
    #Load custom reader if specified, use default otherwise
    if CUSTOM_READER_KEY in arg_table:
        reader_file = "Ingest." + arg_table[CUSTOM_READER_KEY]
    else:
        reader_file = DEFAULT_DATA_READER
    
    raw_to_dict = importlib.import_module(reader_file)
    fn_out = output_dir + fn_table + '.basic.pckl' #serialized file goes to: serialized/jounalEntries.csv.basic.pckl
    data_table = mi.read_raw_file(input_file, raw_to_dict.get_fields, debug)
    mt.save_msg(data_table, fn_out)
    data_loaded = True
        
# Add "simple" metadata to the messages
simple_loaded = False

if (do_simple_metadata or (do_topic and not input_file.endswith(SIMPLE_META_EXTENSION))):
    print 'Extracting simple meta data ...'
    # Load data if it's not loaded
    #NOTE: This block assumes the pickle file has been created
    if (not data_loaded):
        if not input_file.endswith(BASIC_EXTENSION):
            print('ERROR: Simple metadata extraction requires the data pickle file (<name>.{}). Input file {}'.format(BASIC_EXTENSION, input_file))
            sys.exit()
        data_table = mt.load_msg(input_file)
        data_loaded = True

    # Now iterate through the rows (like a DB cursor) and do various operations
    for ky in data_table.keys():
        # Get current transaction
        xact = data_table[ky]
        # Data specific normalization
        text_norm.normalize_msg(xact, debug)
        # Language ID
        mt.msg_lid(xact, debug)

    # Save output of simple meta data
    fn_out = output_dir + fn_table + '.simple.pckl'
    mt.save_msg(data_table, fn_out)
    simple_loaded = True

if (do_topic):
    print 'Performing topic clustering ...'
    # Extract arguments needed
    num_topics = arg_table.get(NUM_TOPICS_KEY,50)
    stop_list = arg_table.get(STOP_LIST_KEY, dir_topic + '/data/stop_list_kiva.txt')
    tf_cutoff = arg_table.get(TF_CUTOFF_KEY, 3)
    df_cutoff = arg_table.get(DF_CUTOFF_KEY, 0.25)
    language_filter = arg_table.get(LANG_FILTER_KEY,'en')
    
    # Load data if it's not loaded
    if (not simple_loaded):
        if not input_file.endswith(SIMPLE_META_EXTENSION):
            print('ERROR: Invalid input file for topic clustering. Use either the raw data and provide the appropriate ingestion module, or use a pre-generated {} file'.format(SIMPLE_META_EXTENSION))
            sys.exit()
        data_table = mt.load_msg(input_file)
        simple_loaded = True

    # Normalization and counts
    rw_hash = mt.create_utf8_rewrite_hash()
    num = 0
    print "Finding counts: ",
    for ky in data_table.keys():
        if ((num % 1000)==0):
            print "{} ".format(num),
            sys.stdout.flush()
        xact = data_table[ky]
        if (xact['lid_lui'] != language_filter):  # do topic only English only
            continue
        # Topic normalization
        topic_norm.normalize_msg(xact, rw_hash, debug)
        # Get counts
        xact['counts'] = mt.get_counts(xact['msg_topic'])
        num += 1
    print

    # Write out counts to a file and perform topic clustering
    fn_counts = dir_temp + fn_table + '.{}.counts.txt'.format(num_topics)
    
    if (os.path.exists(fn_counts)):
        os.remove(fn_counts)
    mt.write_counts_file(data_table, fn_counts)
    
    # Run topic clustering binary
    fn_feat = dir_temp + fn_table + '.{}.feat.txt'.format(num_topics)
    fn_model = dir_temp + fn_table + '.{}.plsa'.format(num_topics)
    cmd = '{}/bin/plsa_estimation_combined_file -vector_list_in {} '.format(dir_topic, fn_counts) + \
        '-stop_list_in {} '.format(stop_list) + \
        '-tf {} -df {} -num_topics {} -random '.format(tf_cutoff,df_cutoff,num_topics) + \
        '-feature_list_out {} '.format(fn_feat) + \
        '-plsa_model_out {} '.format(fn_model)
    print('Running command: {}'.format(cmd))
    status = os.system(cmd)
    if (status != 0):
        print 'Topic clustering failed!'
    else:
        print 'Topic clustering succeeded!'
        fn_summary = output_dir + fn_table + '.{}.summary.txt'.format(num_topics)
        cmd = '{}/bin/plsa_analysis -plsa_model_in {} '.format(dir_topic, fn_model) + \
            ' -summarize > {}'.format(fn_summary)
        print('Running command: {}'.format(cmd))
        status = os.system(cmd)
        fn_d2z = output_dir + fn_table + '.{}.d2z.txt'.format(num_topics)
        cmd = '{}/bin/plsa_analysis -plsa_model_in {} '.format(dir_topic, fn_model) + \
              '-d2z {}'.format(fn_d2z)
        print('Running command: {}'.format(cmd))
        status = os.system(cmd)
        
    fn_out = output_dir + fn_table + '.simple.counts.pckl'
    mt.save_msg(data_table, fn_out)
