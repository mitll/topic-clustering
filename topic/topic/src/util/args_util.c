/***---------------------------------------------------------------------------
* NAME:		llspeech_args
* PURPOSE:	command line argument processor
* DESCRIPTION: 
* 	See Dr. Dobbs Toolbook of C, QA76.73.C15 D7 1986 pp 465-477
* CREATED: 8-Jul-86 rpl                             MIT LINCOLN LABORATORY
* MODIFIED: 27mar89 maz
* MODIFIED: 1jun00 lck
* MODIFIED: 30may09 tjh to fit into tjh's topic/keyword spotting code base
--------------------------------------------------------------------------***/
/** 
    TJH - This argument parser is a modified version of the llspeech
    argument parser with calls into llspeech removed or replaced by 
    calls to the util functions in my topic/keyword library

    @section OldStuff Previous versions of the documentation

    LCK - June 2000

    this code is taken from Marc Zissman's zargs. The major change is that
    this version can support partial variable names. That is, you can give
    the first n characters of a variable name, provided that is enough letters
    to make the variable unique. The actual change is to use a strncmp instead
    of a strcmp, and to check flags for uniqueness.

    For example:
    myprog -inputfile FILE1 -outputfile FILE2 -outformat PCM -linear
    could be called as:
    myprog -i FILE1 -outp FILE2 -outf PCM -l

    LCK - July 2001
    To make the argument parser even more flexible, we would like to
    make it a singly linked list that can be gradually built by each of
    the component modules of an algorithm.
    That is, when doing say xtalk, the main will add some flags to do
    with files and headers, and xtalk itself will add flags to do with
    its algorithm. If we were to combine this with parse automatically,
    parse would then add any flags it would need, like the decimation
    amount.

    The plan is that the arg table is built by having modules add their
    own parts, then the command line arguments are interpreted, then
    the whole arg table is passed to every module on init. The module
    finds the arguments it added and does with them what it
    wants. There is then no need to send the interpreted arguments on
    the stack, simplifying the call structure.

    It's at least a good theory. (And it has been working out well for
    us as of 2007.

    @author
    Marc Zissman's adaptation of Rich Lippmann's adaptation of a
    similar program in Dr. Dobbs Toolbook of C, QA76.73.C15 D7 1986
    pp465-477. Ed Hofstetter added lower and upper bound checking,
    which Linda Kukolich removed. Linda Kukolich added partial
    arguments and the new method of gradually building the argument
    table, module by module.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "util/basic_util.h"
#include "util/args_util.h"

static int check_for_dup_args(ARG_TABLE *arg_tab, ARG *nargp);
static 	ARG	*findarg(const char *name, ARG *tabp);
static void setarg(ARG *argp, const char *value);
static ARG_TABLE *newArg(ARG_TABLE *tabp, const char *name, ARG_TYPE type,
			 ARG_VALUE value, const char *help_text);
static void set_typed_arg(ARG_TABLE *argp, const char *name, ARG_TYPE type,
			  ARG_VALUE value);
static char *StringConcat(char * s1, char * s2);
static char *get_token_from_string(char *line, const char *delim, char *token);
static int count_tokens_in_string(char *line, const char *delim);

/** setarg - Set an argument
    @param argp argument to set
    @param value ascii for the value to set argp to
    @exception argp is NULL
    @exception value is NULL
    @exception bad arugment type
    @exception allocation errors
*/
static void setarg(ARG *argp, const char *value)
{
  if(!argp) return;
  if(!value && argp->type != FLAG_ARG){
    if(argp->arg)
      die ("setarg: value for argument %s is NULL\n", argp->arg);
    else
      die ("setarg: called for an unnamed argument with no value\n");
    return;
  }
  switch(argp->type)
    {
    case INT_ARG:
      argp->value.intV = strtol(value, (char **)NULL, 10);
      break;
    case FLOAT_ARG:
      argp->value.floatV = (float)strtod(value, (char **)NULL);
      break;
    case DOUBLE_ARG:
      argp->value.doubleV = strtod(value, (char **)NULL);
      break;
    case FLAG_ARG:
      argp->value.flagV = 1;
      break;
    case CHAR_ARG:
      argp->value.charV = value[0];
      break;
    case STRING_ARG:
      if (argp->value.stringV) free((void*)argp->value.stringV);
      argp->value.stringV = strdup(value);
      break;
    default:
      die ("llspeech_args: Bad Argument Type\n");
    }
}
/** findarg - find argument
    @param name the name of the argument you are looking for
    @param tabp the first entry in the ARG_TABLE you are looking for
    @return The argument you were looking for. If is is not found,
    NULL is returned

   Changed by LCK as follows:
   check just the beginning of all the names, seeing if there are multiple matches
   @exception ambiguous argument name
*/
static 	ARG	*findarg(const char *name, ARG *tabp)
{
  ARG *retTab = NULL;
  int nameLen = strlen(name);

  while(tabp){
    if (strncmp(tabp->arg,name, nameLen) == 0){
      if(retTab){/* we have an ambiguous match */
 	die ("Option %s is ambiguous (%s, %s)\n", name, retTab->arg, tabp->arg);
      }
      retTab = tabp;
    }
    tabp = tabp->next;
  }
  return (retTab); /* this is NULL if we never found one */
}
/** 
    @brief Print the argument table for user

   Prints in form 
   LOG: -arg type	error_message (value: *variable)

   @param arg_tab - argument table to print
   @exception error in sprintf
*/
void llspeech_args_prusage(ARG_TABLE *arg_tab)
{
  char logMessage[256];
  ARG *tabp;

  /* Find maximum length of the longest command line option string */
  int width;
  int max_width = 0;
  for( tabp = arg_tab->start; tabp; tabp = tabp->next ) {
    switch(tabp->type) {
      case INT_ARG:
	width = strlen(tabp->arg) + 1;
	if (width > max_width) max_width = width;
	break;
      case FLOAT_ARG:
	width = strlen(tabp->arg) + 1;
	if (width > max_width) max_width = width;
        break;
      case DOUBLE_ARG:
	width = strlen(tabp->arg) + 1;
	if (width > max_width) max_width = width;
	break;
      case FLAG_ARG:
	width = strlen(tabp->arg) + 1;
	if (width > max_width) max_width = width;
	break;
      case CHAR_ARG:
	width = strlen(tabp->arg) + 1;
	if (width > max_width) max_width = width;
	break;
      case STRING_ARG:
	width = strlen(tabp->arg) + 1;
	if (width > max_width) max_width = width;
	break;
    }
  }

  /* Print each option */
  for( tabp = arg_tab->start; tabp; tabp = tabp->next ) {
      switch(tabp->type)
	{
	case INT_ARG:
	  if(snprintf(logMessage,255,"-%-*s <int>    %-40s (value: %-5d)\n",
		      max_width, tabp->arg,tabp->errmsg, tabp->value.intV) < 0)
	    die ("llspeech_args_prusage: error in sprintf\n");
	  break;
	case FLOAT_ARG:
	  if(snprintf(logMessage,255,"-%-*s <float>  %-40s (value: %-5g)\n",
		      max_width, tabp->arg,tabp->errmsg, tabp->value.floatV) < 0)
	    die ("llspeech_args_prusage: error in sprintf\n");
	  break;
	case DOUBLE_ARG:
	  if(snprintf(logMessage,255,"-%-*s <double> %-40s (value: %-5g)\n",
		      max_width, tabp->arg,tabp->errmsg,tabp->value.doubleV) < 0)
	    die ("llspeech_args_prusage: error in sprintf\n");
	  break;
	case FLAG_ARG:
	  if(snprintf(logMessage,255,"-%-*s <flag>   %-40s (value: %-5s)\n",
		      max_width, tabp->arg,tabp->errmsg, tabp->value.flagV ? "TRUE":"FALSE") < 0)
	    die ("llspeech_args_prusage: error in sprintf\n");
	  break;
	case CHAR_ARG:
	  if(snprintf(logMessage,255,"-%-*s <char>   %-40s (value: %c)\n",
		      max_width, tabp->arg,tabp->errmsg,tabp->value.charV ) < 0)
	    die ("llspeech_args_prusage: error in sprintf\n");
	  break;
	case STRING_ARG:
	  if(tabp->value.stringV == NULL){
	    if(snprintf(logMessage,255,"-%-*s <str>    %-40s (value: \"(null)\")\n",
			max_width, tabp->arg,tabp->errmsg) < 0)
	      die ("llspeech_args_prusage: error in sprintf\n");
	  } else {
	    if(snprintf(logMessage,255,"-%-*s <str>    %-40s (value: \"%s\")\n",
			max_width, tabp->arg,tabp->errmsg, tabp->value.stringV) < 0)
	      die ("llspeech_args_prusage: error in sprintf\n");
	  }
	  break;
	}
      fprintf(stderr,"%s",logMessage);
    }
}

/**
   make a single string holding all the current values for arguments
   in this arg_tab. The string looks like a command line. This string
   must be freed by the caller.
   @param arg_tab - argument table to print
   @return The string allocated to hold all the flags.
   <b>Note</b> that this string must be freed by the caller
 */
char *llspeech_args_get_flags(ARG_TABLE *arg_tab){
    char *all_flags;
    int DUM_LEN = 256;
    char dummy[DUM_LEN];
    ARG *argp;
    
    if(!arg_tab) return strdup("");

    argp = arg_tab->start;
    all_flags = NULL;
    while(argp){
      switch(argp->type) {
      case INT_ARG:
	snprintf(dummy, DUM_LEN, " -%s %d", argp->arg, argp->value.intV);
	all_flags = StringConcat(all_flags, dummy);
	break;
      case FLOAT_ARG:
	snprintf(dummy, DUM_LEN, " -%s %g", argp->arg, argp->value.floatV);
	all_flags = StringConcat(all_flags, dummy);
	break;
      case DOUBLE_ARG:
	snprintf(dummy, DUM_LEN, " -%s %g", argp->arg, argp->value.doubleV);
	all_flags = StringConcat(all_flags, dummy);
	break;
      case FLAG_ARG:
	/* if flag set, print it in all cases */
	if ( argp->value.flagV > 0) {
	  snprintf(dummy, DUM_LEN, " -%s", argp->arg);
	  all_flags = StringConcat(all_flags, dummy);
	}
	break;
      case CHAR_ARG:
	snprintf(dummy, DUM_LEN, " -%s %c", argp->arg, argp->value.charV);
	all_flags = StringConcat(all_flags, dummy);
	break;
      case STRING_ARG:
	if(argp->value.stringV != NULL &&
	   strcmp(argp->value.stringV, "") != 0){
	  snprintf(dummy, DUM_LEN, " -%-s %s", argp->arg,
		   argp->value.stringV );
	  all_flags = StringConcat(all_flags, dummy);
	}
	break;
      }
      argp = argp->next;
    }
    return(all_flags);
}


/** 
    @brief Process command line arguments

    The steps of processing are: 
    -# Looks for and interprets a configfile to read default
    settings from.
    -# Strips all command line switches and their arguments out of argv,
    setting entries in the arg_tab accordingly

    If an error is found llspeech_exit is called and a usage message is
    put in the log showing all arguments in the table and their
    settings at the time of the error.

    Possible errors are:

    - -- or -help
    If one of the arguments in argv is "--" or "-help", all of the
    help text entries for arg_tab are sent to the log and a fatal
    error will be set, generally exiting the program
   
    - Bad flag
    If there is an argument in argv which does not match any arguments
    in arg_tab, all help text entries will be sent to the log and the
    "bad flag" fatal error will be set, generally exiting the program.

    - Ambiguous argument
    If an argument in argv matches two or more entries, an error will
    be set and the program will exit or do a throw. For example, if
    arg_tab contains entries for -file1 and -file2 and argv has an
    entry for -file, the "ambiguous" error will be set.

    @param argc - length of argv
    @param argv - an array of tokens from the command line
    @param arg_tab - the table to use to interpret argv and to store
    the values of the arguments in
    @return the new argc, after subtracting off for the entries we
    stripped out

    * Changed from Dr Dobbs by MAZ as follows:
    *
    * 	(1) command line switches must begin with a '-', but can be arbitrarily
    *		long.  
    *
    *	(2) the command line consists of the function name followed by
    *		(a) pairs of non-boolean switches and values
    *		(b) boolean switches	
    *		(c) other strings which get passed back to the calling program
    *		
    @exception allocation errors
    @exception ambiguous argument name
 */
int llspeech_args(int argc, char **argv, ARG_TABLE *arg_tab)
{
  int 	nargc;
  char **nargv;
  ARG *argp, *tabp;
  char	*switch_name;
  char	*switch_val;

  if(!arg_tab) return argc;

  tabp = arg_tab->start;

  nargc = 1;
  for (nargv = ++argv; --argc > 0; argv++) {
    if ( **argv != '-' ) {
      *nargv++ = *argv;
      nargc++;
    }
    else if ((strcmp(*argv, "--") == 0) ||
	     (strcmp(*argv, "-help") == 0)) {
      /* this is a way for the user to print the possible args */
      fprintf(stderr, "Possible arguments are:\n");
      llspeech_args_prusage (arg_tab);
      exit(0);
    }
    else {
      switch_name=(*argv)+1; /* move past the minus */
      argp = findarg(switch_name, tabp);
      if (argp == NULL) {
	fprintf (stderr,"llspeech_args: bad flag \"-%s\".\n", switch_name);
	fprintf (stderr, "Allowable flags are:\n");
	llspeech_args_prusage (arg_tab);
	exit(-1);
      }
      else if (argp->type == FLAG_ARG) {
	setarg(argp, NULL);
      }
      else {
	--argc;
	if (argc <= 0) {
	  fprintf ( stderr, "llspeech_args: flag \"%s\" must have an argument\n", switch_name);
	  llspeech_args_prusage (arg_tab);
	  exit(-1);
	}
	else {
	  ++argv;
	  switch_val = *argv;
	  setarg(argp, switch_val);
	}
      }
    }
  }
  return nargc;
}

/** @defgroup newArg Adding arguments to an ARG_TABLE
    @ingroup llspeech_args

    For each of these commands, a new entry is appended to the given
    ARG_TABLE. If the given ARG_TABLE is NULL, a new table is created
    to hold the new entry. The ARG_TABLE with its new entry is
    returned.

    The argument name is checked for uniqueness
    compared to the other argument names already in the table. If it
    is not unique, this command will fail and a fatal error will be
    set. For example, if you try to insert an argument named -out and
    there already is an argument named -output, or vice versa, the
    command will fail.

    A default value is given for the argument. There is also a
    string containing help text which is printed on request by the
    user or when there is an error building the ARG_TABLE or
    processing the command line.

    @exception Allocation errors
    @exception Ambiguous flag
*/
/** @ingroup newArg
    Add a new argument to the given ARG_TABLE
    @param tabp - the ARG_TABLE. If this is NULL, a new one is created
    and returned
    @param name - the name of the new argument. It must be unique in
    the ARG_TABLE
    @param type - the type of the new argument (INT_ARG, FLOAT_ARG,
    etc)
    @param value - the default value for the new argument
    @param help_text - help text to print for this argument when there
    is an error in processing the ARG_TABLE or when requested by the
    user
    @return tabp with a new entry in it
*/
static ARG_TABLE *newArg(ARG_TABLE *tabp, const char *name, ARG_TYPE type,
			 ARG_VALUE value, const char *help_text){
  ARG *argp;

  if(!tabp){
    tabp = (ARG_TABLE *)calloc(1, sizeof(ARG_TABLE));

    /* there are some args that all programs need to have.
       This is a nice central location for adding them */

    // Verbose setting and config file loading not currently activated for this library
    // ARG_VALUE v2;
    // v2.stringV = NULL;
    //tabp = newArg(tabp, CONFIG_ARG, STRING_ARG, v2, "File with -arg value pair on each line");
    //v2.intV = 0;
    //tabp = newArg(tabp, VERBOSE_ARG, INT_ARG, v2, "Chatty info about processing on stdout (higher value more info)");

  }
  argp = (ARG *)calloc(1, sizeof(ARG));
  argp->arg = strdup(name);
  argp->errmsg = strdup(help_text);

  /* this actually will fatal error if this arg is a duplicate */
  if(check_for_dup_args(tabp, argp)) return NULL;

  if(!tabp->start){
    tabp->start = tabp->end = argp;
  } else {
    tabp->end = tabp->end->next = argp;
  }

  argp->type = type;
  switch(type){
  case INT_ARG:
    argp->value.intV = value.intV;
    break;
  case FLOAT_ARG:
    argp->value.floatV = value.floatV;
    break;
  case FLAG_ARG:
    argp->value.flagV = 0;
    break;
  case CHAR_ARG:
    argp->value.charV = value.charV;
    break;
  case STRING_ARG:
    if(value.stringV) argp->value.stringV = (const char *)strdup(value.stringV);
    else argp->value.stringV = NULL;
    break;
  case DOUBLE_ARG:
    argp->value.doubleV = value.doubleV;
    break;
  }
  return tabp;
}

/** @ingroup newArg
    Append an integer argument to tabp
    @param tabp - The existing arguments 
    @param name - the name of this argument
    @param value - the default value for this argument
    @param help_text - help text to print about this argument
    @return The ARG_TABLE with this new entry added to it
*/
ARG_TABLE *llspeech_new_int_arg(ARG_TABLE *tabp, const char *name, int value,
				const char *help_text){
  ARG_VALUE v;
  v.intV = value;
  return newArg(tabp, name, INT_ARG, v, help_text);
}

/** @ingroup newArg
    Append a single precision floating point argument to tabp.

    @param tabp - The existing arguments 
    @param name - the name of this argument
    @param value - the default value for this argument
    @param help_text - help text to print about this argument
    @return The ARG_TABLE with this new entry added to it
*/
ARG_TABLE *llspeech_new_float_arg(ARG_TABLE *tabp, const char *name, float value,
		       const char *help_text){
  ARG_VALUE v;
  v.floatV = value;
  return newArg(tabp, name, FLOAT_ARG, v, help_text);
}
/** @ingroup newArg
    Append a new flag argument to tabp.

    @param tabp - The existing arguments 
    @param name - the name of this argument
    @param help_text - help text to print about this argument
    @return The ARG_TABLE with this new entry added to it
*/
ARG_TABLE *llspeech_new_flag_arg(ARG_TABLE *tabp, const char *name, const char *help_text){
  ARG_VALUE v;
  v.flagV = 0;
  return newArg(tabp, name, FLAG_ARG, v, help_text);
}
/** @ingroup newArg
    Append a character argument to tabp.

    @param tabp - The existing arguments 
    @param name - the name of this argument
    @param value - the default value for this argument
    @param help_text - help text to print about this argument
    @return The ARG_TABLE with this new entry added to it
*/
ARG_TABLE *llspeech_new_char_arg(ARG_TABLE *tabp, const char *name, char value,
		      const char *help_text){
  ARG_VALUE v;
  v.charV = value;
  return newArg(tabp, name, CHAR_ARG, v, help_text);
}
/** @ingroup newArg
    Append a string argument to tabp.

    @param tabp - The existing arguments 
    @param name - the name of this argument
    @param value - the default value for this argument
    @param help_text - help text to print about this argument
    @return The ARG_TABLE with this new entry added to it
*/
ARG_TABLE *llspeech_new_string_arg(ARG_TABLE *tabp, const char *name, const char *value,
			const char *help_text){
  ARG_VALUE v;
  v.stringV = value;
  return newArg(tabp, name, STRING_ARG, v, help_text);
}
/** @ingroup newArg
    Append a double precision floating point argument to tabp.

    @param tabp - The existing arguments 
    @param name - the name of this argument
    @param value - the default value for this argument
    @param help_text - help text to print about this argument
    @return The ARG_TABLE with this new entry added to it
*/
ARG_TABLE *llspeech_new_double_arg(ARG_TABLE *tabp, const char *name, double value,
			const char *help_text){
  ARG_VALUE v;
  v.doubleV = value;
  return newArg(tabp, name, DOUBLE_ARG, v, help_text);
}

/** @defgroup setArg Setting values in ARG_TABLE
    @ingroup llspeech_args
    These routines allow the user to modify entries in the
    ARG_TABLE. This can be useful in ensuring that multiple values
    make sense together, and allows one module to set arguments for
    the use of other modules. For example, in llspeech data formats
    are read from file headers, but can also be set on the command
    line. The header reading module modifies these ARG_TABLE entries
    to match what is found in the data file header. This way, other
    parts of the system can use the setting from the ARG_TABLE as a
    single accurate source of file format information.

    Each routine searches the ARG_TABLE for an argument that matches
    the given name and has the correct type. The name may be an
    abbreviation of the name for its entry, though it must be
    sufficiently long to identify a single ARG_TABLE entry.

    @exception NULL argument name
    @exception ambiguous argument name
    @exception argument not found
    @exception argument is of the wrong type
 */
/** Set the value for an argument named name in the table argp
    @param argp - The argument table
    @param name - the name of this argument
    @param type - which kind of argument this is (INT_ARG, FLOAT_ARG, etc)
    @param value - the default value for this argument
   @exception ambiguous argument name
*/
static void set_typed_arg(ARG_TABLE *argp, const char *name, ARG_TYPE type,
			  ARG_VALUE value)
{
  ARG *my_arg;

  if(!name) {
    die("llspeech_set_arg: cannot retrieve value for NULL name\n");
  }
  my_arg = findarg(name, argp->start);
  if(!my_arg) {
    die ("llspeech_set_arg: cannot find argument named %s\n", name);
  }
  if(my_arg->type != type){
    die("llspeech_set_arg: arg %s is the wrong type\n", name);
  }
  switch(type){
  case INT_ARG:
    my_arg->value.intV = value.intV;
    break;
  case FLOAT_ARG:
    my_arg->value.floatV = value.floatV;
    break;
  case FLAG_ARG:
    my_arg->value.flagV = value.flagV;
    break;
  case CHAR_ARG:
    my_arg->value.charV = value.charV;
    break;
  case STRING_ARG:
    if (my_arg->value.stringV) free((void*)my_arg->value.stringV);
    if(value.stringV) my_arg->value.stringV = strdup(value.stringV);
    else my_arg->value.stringV = NULL;
    break;
  case DOUBLE_ARG:
    my_arg->value.doubleV = value.doubleV;
    break;
  }
}
/** @ingroup setArg
    Set the value for an integer argument named name in the table tabp
    @param tabp - the argument table
    @param name - the name of the argument which we want to change the value of
    @param value - the new value for the argument "name"
*/
void llspeech_set_int_arg(ARG_TABLE *tabp, const char *name, int value){
  ARG_VALUE v;
  v.intV = value;
  set_typed_arg(tabp, name, INT_ARG, v);
}
/** @ingroup setArg
    Set the value for a single precision floating point argument
    named name in the table tabp
    @param tabp - the argument table
    @param name - the name of the argument which we want to change the value of
    @param value - the new value for the argument "name"
*/
void llspeech_set_float_arg(ARG_TABLE *tabp, const char *name, float value){
  ARG_VALUE v;
  v.floatV = value;
  set_typed_arg(tabp, name, FLOAT_ARG, v);
}
/** @ingroup setArg
    Set the value for a flag argument named name in the table tabp
    @param tabp - the argument table
    @param name - the name of the argument which we want to change the value of
    @param value - the new value for the argument "name"
*/
void llspeech_set_flag_arg(ARG_TABLE *tabp, const char *name, int value){
  ARG_VALUE v;
  v.flagV = value;
  set_typed_arg(tabp, name, FLAG_ARG, v);
}
/** @ingroup setArg
    Set the value for a character argument named name in the table tabp
    @param tabp - the argument table
    @param name - the name of the argument which we want to change the value of
    @param value - the new value for the argument "name"
*/
void llspeech_set_char_arg(ARG_TABLE *tabp, const char *name, char value){
  ARG_VALUE v;
  v.charV = value;
  set_typed_arg(tabp, name, CHAR_ARG, v);
}
/** @ingroup setArg
    Set the value for a string argument named name in the table
    tabp. The string given is duplicated for inclusion in the table,
    so the user is responsible for destroying the original.

    @param tabp - the argument table
    @param name - the name of the argument which we want to change the value of
    @param value - the new value for the argument "name"
*/
void llspeech_set_string_arg(ARG_TABLE *tabp, const char *name, char *value){
  ARG_VALUE v;
  v.stringV = value;
  set_typed_arg(tabp, name, STRING_ARG, v);
}
/** @ingroup setArg
    Set the value for a double precision floating point  argument
    named name in the table tabp
    @param tabp - the argument table
    @param name - the name of the argument which we want to change the value of
    @param value - the new value for the argument "name"
*/
void llspeech_set_double_arg(ARG_TABLE *tabp, const char *name, double value){
  ARG_VALUE v;
  v.doubleV = value;
  set_typed_arg(tabp, name, DOUBLE_ARG, v);
}

/**
   Is this argument's name unique enough? Shortened strings, starting
   from the first character of the name are allowed. For example, the
   flag -inFile can be given as -in. Therefore, there cannot be
   another flag -i, which would be confused with -inFile.
   @return 1 if there is a duplicate or substring in such a way as to
   confuse the argument parser later on
   @exception NULL name
   @exception ambiguous arg name
*/
static int check_for_dup_args(ARG_TABLE *arg_tab, ARG *nargp){
  ARG *argp;
  int l1, l2;

  if(!nargp->arg){
    if(nargp->errmsg){
      die ("New arg has no name. Help text is %s\n", nargp->errmsg);
    } else {
      die("New arg has no name.\n");
    }
  }
  l2 = strlen(nargp->arg);

  argp = arg_tab->start;
  while(argp){
    l1 = strlen(argp->arg);
    if(((l1 <= l2) && !strncmp(argp->arg, nargp->arg, l1)) ||
       ((l1 > l2) && !strncmp(argp->arg, nargp->arg, l2))){
      die ("New argument is confusable with another: -%s and -%s\n",
		    argp->arg, nargp->arg);
      return 1;
    }
    argp = argp->next;
  }
  return 0;
}

/** @defgroup getArg Getting values from ARG_TABLE
    @ingroup llspeech_args

    Get the stored value for a named argument in the given ARG_TABLE. The name
    given may be an abbreviation of the name in the table, though is
    must be sufficiently long to identify a single ARG_TABLE entry. If
    a string value is requested, the value returned is the actual
    string stored in the ARG_TABLE. It should not be freed by the
    user.

    @exception NULL name
    @exception Cannot find the named entry in the ARG_TABLE
    @exception ambiguous arg name
    @exception argument is of the wrong type
*/
/** @ingroup getArg
    Get the value of an integer argument
    @param argp - the table of arguments
    @param name - the name of the argument which you want the value
    for
    @return the value of name in argp
 */
int llspeech_get_int_arg(ARG_TABLE *argp, const char *name){
  ARG *my_arg;

  if(!argp) return 0;
  if(!name){
    die("llspeech_get_int_arg: cannot retrieve value for NULL name\n");
  }
  my_arg = findarg(name, argp->start);
  if(!my_arg){
    die("llspeech_get_int_arg: cannot find argument named %s\n", name);
  }
  if(my_arg->type != INT_ARG){
    die("llspeech_get_int_arg: arg %s is not a int\n", name);
  }
  return my_arg->value.intV;
}
/** @ingroup getArg
    Get the value of a single precision floating point argument
    @param argp - the table of arguments
    @param name - the name of the argument which you want the value
    for
    @return the value of name in argp
 */
float llspeech_get_float_arg(ARG_TABLE *argp, const char *name){
  ARG *my_arg;

  if(!argp) return 0.;

  if(!name){
    die("llspeech_get_float_arg: cannot retrieve value for NULL name\n");
  }
  my_arg = findarg(name, argp->start);
  if(!my_arg){
    die("llspeech_get_float_arg: cannot find argument named %s\n", name);
  }
  if(my_arg->type != FLOAT_ARG){
    die("llspeech_get_float_arg: arg %s is not a float\n", name);
  }
  return my_arg->value.floatV;
}

/** @ingroup getArg
    Get the value of a double precision floating point argument
    @param argp - the table of arguments
    @param name - the name of the argument which you want the value
    for
    @return the value of name in argp
 */
double llspeech_get_double_arg(ARG_TABLE *argp, const char *name){
  ARG *my_arg;

  if(!argp) return 0.;

  if(!name){
    die("llspeech_get_double_arg: cannot retrieve value for NULL name\n");
  }
  my_arg = findarg(name, argp->start);
  if(!my_arg){
    die("llspeech_get_double_arg: cannot find argument named %s\n", name);
  }
  if(my_arg->type != DOUBLE_ARG){
    die("llspeech_get_double_arg: arg %s is not a double\n", name);
  }
  return my_arg->value.doubleV;
}

/** @ingroup getArg
    Get the value of a flag argument
    @param argp - the table of arguments
    @param name - the name of the argument which you want the value
    for
    @return the value of name in argp
 */
int llspeech_get_flag_arg(ARG_TABLE *argp, const char *name){
  ARG *my_arg;

  if(!argp) return 0;

  if(!name){
    die("llspeech_get_flag_arg: cannot retrieve value for NULL name\n");
  }
  my_arg = findarg(name, argp->start);
  if(!my_arg){
    die("llspeech_get_flag_arg: cannot find argument named %s\n", name);
  }
  if(my_arg->type != FLAG_ARG){
    die("llspeech_get_flag_arg: arg %s is not a flag\n", name);
  }
  return my_arg->value.flagV;
}

/** @ingroup getArg
    Get the value of a character argument
    @param argp - the table of arguments
    @param name - the name of the argument which you want the value
    for
    @return the value of name in argp
 */
char llspeech_get_char_arg(ARG_TABLE *argp, const char *name){
  ARG *my_arg;

  if(!argp) return '\0';

  if(!name){
    die("llspeech_get_char_arg: cannot retrieve value for NULL name\n");
  }
  my_arg = findarg(name, argp->start);
  if(!my_arg){
    die("llspeech_get_char_arg: cannot find argument named %s\n", name);
  }
  if(my_arg->type != CHAR_ARG){
    die("llspeech_get_char_arg: arg %s is not a char\n", name);
  }
  return my_arg->value.charV;
}

/** @ingroup getArg
    Get the value of a string argument
    @param argp - the table of arguments
    @param name - the name of the argument which you want the value
    for
    @return the value of name in argp
    @exception ambiguous argument name
 */
const char *llspeech_get_string_arg(ARG_TABLE *argp, const char *name){
  ARG *my_arg;

  if(!argp) return NULL;

  if(!name){
    die("llspeech_get_string_arg: cannot retrieve value for NULL name\n");
  }
  my_arg = findarg(name, argp->start);
  if(!my_arg){
    die("llspeech_get_string_arg: cannot find argument named %s\n", name);
  }
  if(my_arg->type != STRING_ARG){
    die("llspeech_get_string_arg: arg %s is not a string\n", name);
  }
  return my_arg->value.stringV;
}

/** @brief Free the ARG_TABLE

    Free all the entries in arg_tab. For those string entries which
    have an associated value, that associated string is also freed.

    @param arg_tab - the ARG_TABLE to free
*/
void free_arg_table(ARG_TABLE *arg_tab){
  ARG *nargp, *argp;

  if(!arg_tab) return;

  argp = arg_tab->start;
  while(argp){
    nargp = argp->next;
    if (argp->arg) free(argp->arg);
    if (argp->errmsg) free(argp->errmsg);
    switch(argp->type){
    case STRING_ARG:
      if (argp->value.stringV) free((void*)argp->value.stringV);
      break;
    default:
      break;
    }
    if ( argp) free(argp);
    argp = nargp;
  }
  if ( arg_tab) free(arg_tab);
}
/** @defgroup stringArray Comma delimited strings and arrays
    @ingroup llspeech_args
    Convert a string into an typed array or a typed array into a comma
    delimited string.

    In some earlier argument parsing packages used at Lincoln Lab, we
    automatically handled arrays of float, ints or strings. In
    llspeech_args, these arrays are first declared as string arguments
    and then are translated into arrays separately.

    When going from string to array, the string is broken
    wherever there is a comma (,) or a colon (:) character, except
    where those characters are embedded in a double quoted string (")

    There is an argument available to store the length of the
    resulting array. If that argument is NULL, the array will be
    allocated one entry longer and the first entry will be used to
    store the length. It will be stored as whatever type the rest of
    the array is.

    When going from array to string, a string is allocated to be long
    enough to hold all the entries in the array, plus a comma to go
    between each field.

    Note, there is no special provision made to make sure there are
    double quotes around entries in a string array that have spaces in
    them. This may cause problems later, if you want to automatically
    interpret the strings.

    @exception allocation errors
 */

/** @ingroup stringArray
    Convert a string into an array of strings.
    @param original - the string we want to make into an array
    @param lengthP - a place to store the length of the array
    @return an array of strings holding copies of the parts of the
    original string
 */
char **llspeech_make_string_array(const char *original, int *lengthP){
  char **string_array;
  char *copy, *copy2, *next, *token, delim[3] = ",:";
  int i = 0, len;

  if(!original){
    if(lengthP){
      *lengthP = 0;
      return NULL;
    } else {
      string_array = (char **)calloc(1, sizeof(char *));
      string_array[0] = strdup("0");
      return string_array;
    }
  }

  copy = strdup(original);
  len = count_tokens_in_string(copy, delim);

  if(lengthP){
    string_array = (char **)calloc(len, sizeof(char *));
    *lengthP = len;
  } else {
    string_array = (char **)calloc(len+1, sizeof(char *));
    string_array[0] = (char *)malloc(64);
    sprintf(string_array[0], "%d", len);
    i++;
  }

  token = copy;
  copy2 = strdup(original);
  next = get_token_from_string(copy2, delim, token);
  while (strlen(token)) {
    string_array[i++] = strdup(token);
    next = get_token_from_string(next, delim, token);
  }

  if (copy) free(copy);
  if (copy2) free(copy2);
  return string_array;
}

/** @ingroup stringArray
    Free an array of strings and the strings it contains
    @param array - the array of strings
    @param len - the length of the array of strings
*/
void llspeech_free_string_array(char **array, int len){
  int i;
  if(!array) return;
  for(i = 0; i < len; i++){
    if ( array[i] ) free(array[i]);
  }
  if ( array ) free(array);
}

/** @ingroup stringArray
    Convert a string into an array of integers.
    @param original - the string we want to make into an array
    @param array_len - a place to store the length of the array.
    @return an array of the integers in the original string
 */
int *llspeech_make_int_array(const char *original, int *array_len){
  int *int_array;
  char *copy, *copy2, *next, *token, delim[3] = ",:";
  int i = 0, len;

  if(!original){
    if(array_len){
      *array_len = 0;
      return NULL;
    } else {
      int_array = (int *)calloc(1, sizeof(int));
      int_array[0] = 0;
      return int_array;
    }
  }
  copy = strdup(original);
  len = count_tokens_in_string(copy, delim);
  if(array_len){
    *array_len = len;
    int_array = (int *)calloc(len, sizeof(int));
  } else {
    int_array = (int *)calloc(len+1, sizeof(int));
    int_array[0] = len;
    i++;
  }
  token = copy;
  copy2 = strdup(original);
  next = get_token_from_string(copy2, delim, token);
  while(strlen(token)){
    int_array[i++] = atoi(token);
    next = get_token_from_string(next, delim, token);
  }
  if ( copy ) free(copy);
  if ( copy2 ) free(copy2);
  return int_array;
}

/** @ingroup stringArray
    Convert a string into an array of floats.
    @param original - the string we want to make into an array
    @param array_len - a place to store the length of the array.
    @return an array of the floats in the original string
 */
float *llspeech_make_float_array(const char *original, int *array_len){
  float *float_array;
  char *copy, *copy2, *next, *token, delim[3] = ",:";
  int i = 0, len;

  if(!original){
    if(array_len){
      *array_len = 0;
      return NULL;
    } else {
      float_array = (float *)calloc(1, sizeof(float));
      float_array[0] = 0.;
      return float_array;
    }
  }

  copy = strdup(original);
  len = count_tokens_in_string(copy, delim);
  if(array_len){
    *array_len = len;
    float_array = (float *)calloc(len, sizeof(float));
  } else {
    float_array = (float *)calloc(len+1, sizeof(float));
    float_array[0] = len;
    i++;
  }
  token = copy;
  copy2 = strdup(original);
  next = get_token_from_string(copy2, delim, token);
  while(strlen(token)){
    float_array[i++] = atof(token);
    next = get_token_from_string(next, delim, token);
  }
  if ( copy ) free(copy);
  if ( copy2 ) free(copy2);
  return float_array;
}
/** @ingroup stringArray
    Make a single comma delimited string out of an array of ints
    @param array - the array of ints
    @param length - the length of the array
    @return a string representing the integer array
*/
char *llspeech_int_array_to_string(int *array, int length){
  char *ret_string = (char *)calloc(16*length, 1);
  int i;

  for(i = 0; i < length; i++){
    sprintf(ret_string, "%s%d,", ret_string, array[i]);
  }
  ret_string[strlen(ret_string) - 1] = '\0'; /* take off the last comma */
  return ret_string;
}

/** @ingroup stringArray
    Make a single comma delimited string out of an array of floats
    @param array - the array of floats
    @param length - the length of the array
    @return a string representing the float array
*/
char *llspeech_float_array_to_string(float *array, int length){
  char *ret_string = (char *)calloc(16*length, 1);
  int i;

  for(i = 0; i < length; i++){
    sprintf(ret_string, "%s%f,", ret_string, array[i]);
  }
  ret_string[strlen(ret_string) - 1] = '\0'; /* take off the last comma */
  return ret_string;
}

/** @ingroup stringArray
    Make a single comma delimited string out of an array of strings
    @param array - the array of string
    @param length - the length of the array
    @return a string with each of the entries in array, separated by
    commas (,)
 */
char *llspeech_string_array_to_string(char **array, int length){
  char *ret_string;
  int i, total_length = 1;

  for(i = 0; i < length; i++){
    total_length += strlen(array[i]) + 1;
  }
  ret_string = (char *)calloc(total_length, 1);

  for(i = 0; i < length; i++){
    sprintf(ret_string, "%s%s,", ret_string, array[i]);
  }
  ret_string[strlen(ret_string) - 1] = '\0'; /* take off the last comma */
  return ret_string;
}


/**
   Fill an argv style string with tokens taken from a file
   @param argv - command line to fill with the entries from the file
   @param describe_file - file with a command line starting on the
   first line. The command can extend to more lines so long as each
   line ends with a backslash (\) until the final one
   @param max_args - The maximum number of arguments to put into argv
   @return the number of strings stored in argv at the end (argc)
   */
int fill_argv(char **argv, FILE *describe_file, int max_args)
{
  int	argc, t, stop, s, e, eof = 0, start;
  int command_len = 0;
  char *command, c;
  int retC;
  int NNMAX_LINE = 2000;


  /* fill up command with the data */
  command = (char *)malloc(NNMAX_LINE);
  command_len = NNMAX_LINE;

  command[0] = '\0';
  stop = 0;
  start = 1;
  t = 0;
  while(stop == 0){
    retC = getc(describe_file);
    c = (char)retC;

    /* check if command is long enough, if not, make it bigger */
    if(t >= command_len){
      command = (char *)realloc(command, command_len+NNMAX_LINE);
      command_len += NNMAX_LINE;
    }
    if(retC == EOF){
      /* if at the end of a file, stop reading */
      command[t] = '\0';
      stop = 1;
      eof = 1;
    } else if(c == '\n'){
      /* if at the end of a line, check for a backslash to
	 continue or stop reading */
      if(t > 0 && command[t-1] == '\\'){
	command[t-1] = '\0';
	t -= 2; /* overwrite \\ and \n */
      } else if (t > 1 && command[t-1] == '\r'&& command[t-2] == '\\'){
	command[t-2] = '\0';
	t -= 3; /* same thing, DOS format */
      } else {
	command[t] = '\0';
	stop = 1;
      }
    } else {
      /* add the character to the command line */
      if(start){
	if(c == ' ' || c == '\t') t--;
	else {
	  start = 0;
	  command[t] = c;
	}
      } else {
	command[t] = c;
      }
    }
    t++;
  }
  /* fill argv with pointers to the entries in command */
  argc = 0;
  s = 0;
  while(1){
    if(command[s] == '\0') {
      goto finished;
    }
    argv[argc] = &command[s];
    if(argv[argc++][0] == '\"'){ 
      for(e = s+1; e < command_len-1; e++){
	if(command[e] == '\"'){
	  e++;
	  /* if the quote was the last thing on the line */
	  if(command[e] == '\0') goto finished;
	  if(e < command_len) command[e] = '\0';
	  break;
	} else if(command[e] == '\0'){
	  goto finished;
	}
      }
    } else {
      /* regular token */
      for(e = s+1; e < command_len; e++){
	if(command[e] == ' ' || command[e] == '\t'){
	  command[e] = '\0';
	  break;
	} else if(command[e] == '\0') goto finished;
      }
    }
    s = e+1;
    if(command[s] == '\0') break;
  }
 finished:
  if(argc == 0 && eof) return EOF;
  else return argc;
}    

static char *StringConcat(char * s1, char * s2)
{
    int length;
    if(s2 == NULL) return s1;
    if(s1 == NULL){
	s1 = strdup(s2);
    } else {
	length = strlen(s1) + strlen(s2) + 1;
	s1 = (char *)realloc(s1, length);
	strcat(s1, s2);
    }
    return s1;
}

static char *get_token_from_string(char *line, const char *delim, char *token)
{
  char *cptr1, *cptr2;

  if (!line || (*line == '\n') || (*line == '\r') || (*line == '\0')
      || (*line == '#'))  {
    if (token) *token = '\0';
    return((char *)NULL);
  }

  cptr1 = line;
  cptr2 = token;

  // Squeeze out leading delimiters 
  while (strchr(delim,*cptr1)) cptr1++;

  // Double quote enclosed tokens are not parsed on delims
  if (*cptr1 == '\"') {
    cptr1++; // skip first quote 
    while (*cptr1 != '\"' && *cptr1 != '\n' && *cptr1 != '\r' &&
	   *cptr1 != '\0') 
      *cptr2++ = *cptr1++;
    if (*cptr1 == '\"')  cptr1++; // skip last quote 
  } else {
    // Regular tokens are parsed on delims
    while (*cptr1 != '\n' && *cptr1 != '\r' && *cptr1 != '\0' && 
	   !strchr(delim,*cptr1) && *cptr1 != '#')
      *cptr2++ = *cptr1++;

    // If we ended because of a #, make sure the line ends on the
    // next character
    if(*cptr1 == '#'){
      *(cptr1 + 1) = '\0';
    }
  }
	    
  *cptr2 = '\0'; // End token string

  // Squeeze out trailing delimiters 
  if(*cptr1 != '\0')
    while (strchr(delim,*cptr1)) cptr1++;

  // Check for end of line or string 
  if (*cptr1 == '\n' || *cptr1 == '\r' || *cptr1 == '\0') 
    return((char *) NULL);
  else 
    return(cptr1);
}

static int count_tokens_in_string(char *line, const char *delim) 
{
  int count = 0;
  char token[2048], *next;
  next = get_token_from_string(line, delim, token);
  while (strlen(token)) {
	count++;
  	next = get_token_from_string(next, delim, token);
  }
  return(count);
}
