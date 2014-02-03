/** @file args.h
    @brief command line argument processor

    See the @ref llspeech_args module for more.
 */
/**
   @defgroup llspeech_args Command Line Argument Parsing
   @ingroup llspeech_utils

       This is the command line argument parser for the LLSpeech
    system. With it, a table is gradually created containing one entry
    for each command line argument the user wishes to interpret. This
    work can be done by individual modules, each adding its arguments
    in turn. When the whole table is complete, the user uses it to
    interpret a command line using the llspeech_args() command. Each
    module in the system can then
    query this argument table to retrieve the values for those
    arguments it added to the table. In addition, it is possible for
    certain values that are found in the argument table to be
    communicated to other modules by being set in the table, and then
    retrieved elsewhere. This makes the call interfaces to modules
    much simpler than it might be otherwise.

    @section configfile Other input methods

    This system is designed to take in the argv array from the main()
    routine in a C or C++ program. There are two other methods for
    setting the arguments in an ARG_TABLE based on external
    inputs. Both involve storing arguments in a file and interpretting
    the that file.

    The first method is to use a configfile. The string argument
    "-configfile" is inserted into every argument table. This
    configuration file can be used to set any argument in the
    ARG_TABLE. During the call to llspeech_args(), if the -configfile
    argument is set, the file is automatcially opened, read, and
    interpretted, setting values in the ARG_TABLE. After this is
    complete, the command line is interpretted.

    Each line of the configuration file has one command line
    argument. If multiple arguments are on a single line, only the
    first one will be interpretted. For example:

    @code
    -ninputs 38
    -noutputs 2
    -labels MALE,FEMALE
    @endcode

    The second method is to use fill_argv() to read in a command line
    which starts on the first line of a file. This command line may
    continue onto multiple lines by using the backslash (\) character
    at the end of any line. Unlike the configfile, multiple command
    line arguments can be on each line. The first field in the file is
    a dummy command name. For example:

    @code
    Gid.train -ninputs 38 -noutputs 2 \
    -labels MALE,FEMALE
    @endcode
    
    There are also two methods of forcing argument settings
    internally. You can call the routines in the setArg section of
    this manual, or you can create an artifical argv with the
    appropriate flags and values and then send that to
    llspeech_args().

    @section example Example
    @code

    int main(int argc, char **argv){
        ARG_TABLE *argtab = NULL;
        double ais=32768.0;
        float cutoff=100.0;
        int samprate=10000;
        int skip=0;
        char *fnameaf=NULL;
        char *intListS = NULL;
        int *intList, intListLen;

        llspeech_error_init(0);
        argtab = llspeech_new_double_arg(argtab, "ais", ais, "divide by this on input");
        argtab = llspeech_new_float_arg(argtab, "c", cutoff, "cutoff for peak picking in Hz");
        argtab = llspeech_new_int_arg(argtab, "sr", samprate, "sample rate in samp/sec");
        argtab = llspeech_new_flag_arg(argtab, "skip", "skip 512 bytes initially");
        argtab = llspeech_new_string_arg(argtab, "filterFile", fnameaf, "adaptive filter input file name");

        argtab = llspeech_new_string_arg(argtab, "intList", NULL,
"list of integers");

        argc = llspeech_args(argc, argv, argtab);

        llspeech_args_prusage(argtab);

        ais = llspeech_get_double_arg(argtab, "ais");
        samprate = llspeech_get_int_arg(argtab, "sr");

        intListS = llspeech_get_string_arg(argtab, "intList");
        if(intListS)
                intList = llspeech_make_int_array(intListS, &intListLen);

        llspeech_free(intList);

        free_arg_table(argtab);
        llspeech_error_free();
    }
    @endcode


    @section Bugs Bugs

    Arguments that are not flags but that begin with a hyphen can be
    troublesome. Because they have a hyphen in front, they will be
    interpretted as mis-spelled ARG_TABLE entries, causing an error
    when the program runs.

    If the programmer specifies two flags, one with a name that is a
    superset of the other, the partial argument parsing will see that
    the flags are not unique, complain, and stop the program. That is,
    a flag pair like {-out, -output} won't work.

    When using the make_*_array routines, if you have a double quoted
    section that includes a comma or a colon, it will still split on
    that comma or colon. For example: -files ``this one'',``that
    one'',``and the other'' will generate three entries if sent
    through llspeech_make_string_array(), as expected. -files ``C:this
    one'',``and that one'' will produce 3 entries, not 2. (C,``this
    one'',``and that one'').

    When using llspeech_string_array_to_string(), if there are spaces
    in entries in the string array, they are are protected by double
    quotes in the resulting string. So ("this and", "that") will
    result in 'this and,that' rather than '"this and",that'. This may
    cause problems interpretting the strings later.
 */
/***---------------------------------------------------------------------------
* NAME:         args.h
* PURPOSE:      typdefs needed for args command line argument processor
* USAGE: 
*       #include "args.h"
* CREATED: 8-Jul-86 rpl                             MIT LINCOLN LABORATORY
* MODIFIED: 27mar89 maz
--------------------------------------------------------------------------***/
#ifndef ARGS_H
#define ARGS_H

/** @brief Our possible argument type */
typedef enum {
  INT_ARG = 0, ///< Integer
  FLOAT_ARG, ///< Float
  FLAG_ARG, ///< Boolean
  CHAR_ARG, ///< Single character
  STRING_ARG, ///< C string
  DOUBLE_ARG, ///< double
}ARG_TYPE;
/** @brief The value of an arg table entry. Can be any of our types */
typedef union {
  int	intV; ///< Integer value
  float	floatV; ///< Float value
  int	flagV;  ///< boolean value (stored as an int)
  char	charV; ///< character value
  const char	*stringV; ///< string value
  double doubleV; ///< double value
  /*
  INT_LIST *intLV;
  FLOAT_LIST *floatLV;
  POINTER_FIFO *stringLV;
  */
} ARG_VALUE;

/**
   @brief Entry in the argument table
 */
typedef struct argEntry
{
  char		*arg;		/**< Name of this argument */
  ARG_TYPE	type;		/**< Argument type */
  ARG_VALUE	value;		/**< The value of this argument. One
				   of several types stored in a union */
  char		*errmsg;	/**< Help text to print on error */
  struct argEntry *next;	/**< Pointer to next arg in table */
} ARG;

/**
   @brief Argument table, actually a singly linked list of ARGs
*/
typedef struct {
  ARG *start;	/**< Start of the table. For processing */
  ARG *end;	/**< End of the table. For adding on */
} ARG_TABLE;
/// parse the commend line, setting relevant values in the table
int llspeech_args(int argc, char **argv, ARG_TABLE *tabp);
/// print the help text for the table
void llspeech_args_prusage(ARG_TABLE *tabp);
/// add a new entry of the type int to the table
ARG_TABLE *llspeech_new_int_arg(ARG_TABLE *tabp, const char *name, int value,
			   const char *help_text);
/// add a new entry of type float to the table
ARG_TABLE *llspeech_new_float_arg(ARG_TABLE *tabp, const char *name, float value,
			     const char *help_text);
/// add a new flag entry to the table
ARG_TABLE *llspeech_new_flag_arg(ARG_TABLE *tabp, const char *name, const char *help_text);
/// add a new entry of type char to the table
ARG_TABLE *llspeech_new_char_arg(ARG_TABLE *tabp, const char *name, char value,
			    const char *help_text);
/// add a new entry of type char * (string) to the table
ARG_TABLE *llspeech_new_string_arg(ARG_TABLE *tabp, const char *name, const char *value,
			      const char *help_text);
/// add a new entry of type double to the table
ARG_TABLE *llspeech_new_double_arg(ARG_TABLE *tabp, const char *name, double value,
			      const char *help_text);
/// destructor
void free_arg_table(ARG_TABLE *tabp);

/// get the value of a table entry of type int
int llspeech_get_int_arg(ARG_TABLE *tabp, const char *name);
/// get the value of a table entry of type float
float llspeech_get_float_arg(ARG_TABLE *tabp, const char *name);
/// get the value of a table entry of type double
double llspeech_get_double_arg(ARG_TABLE *tabp, const char *name);
/// get the value of a table entry of type char
char llspeech_get_char_arg(ARG_TABLE *tabp, const char *name);
/// get the value of a table entry of type flag
int llspeech_get_flag_arg(ARG_TABLE *tabp, const char *name);
/// get the value of a table entry of type char * (string)
const char *llspeech_get_string_arg(ARG_TABLE *tabp, const char *name);
/// set the value of a table entry of type int
void llspeech_set_int_arg(ARG_TABLE *tabp, const char *name, int value);
/// set the value of a table entry of type float
void llspeech_set_float_arg(ARG_TABLE *tabp, const char *name, float value);
/// set the value of a table entry of type flag
void llspeech_set_flag_arg(ARG_TABLE *tabp, const char *name, int value);
/// set the value of a table entry of type char
void llspeech_set_char_arg(ARG_TABLE *tabp, const char *name, char value);
/// set the value of a table entry of type string
void llspeech_set_string_arg(ARG_TABLE *tabp, const char *name, char *value);
/// set the value of a table entry of type double
void llspeech_set_double_arg(ARG_TABLE *tabp, const char *name, double value);
/// make an array of strings from a comma delimited string
char **llspeech_make_string_array(const char *original, int *lengthP);
/// create an array of integers from a comma delimited string
int *llspeech_make_int_array(const char *original, int *lengthP);
/// create an array of floats from a comma delimited string
float *llspeech_make_float_array(const char *original, int *lengthP);
/// free an array of strings
void llspeech_free_string_array(char **array, int len);
/// make a comma delimited string representing an array of integers
char *llspeech_int_array_to_string(int *array, int length);
/// make a comma delimited string representing an array of floats
char *llspeech_float_array_to_string(float *array, int length);
/// make a single comma delimited string representing an array of strings
char *llspeech_string_array_to_string(char **array, int length);

/// make a string holding all current values for arguments in this table
char *llspeech_args_get_flags(ARG_TABLE *argp);

/// fill an argv style string array with tokens from a file
int fill_argv(char **argv, FILE *describe_file, int max_args);

#define VERBOSE_ARG "verbose" /**< Send more to the log when this is higher */
#define CONFIG_ARG "configfile" /**< Read default settings from this file before processing argv */

#endif /* ARGS_H */
