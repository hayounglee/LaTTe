/*
 * main.c
 * Kick off program.
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 * Modified by MASS Laboratory, SNU, 1999.
 */

#include <time.h>

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "jtypes.h"
#include "native.h"
#include "constants.h"
#include "support.h"
#include "errors.h"
#include "exception.h"
#include "thread.h"
#include "flags.h"
#include "gc.h"

#ifdef TRANSLATOR
#include "stat.h"
#include "CFG.h"
#include "InstrNode.h"
#include "method_inlining.h"
#include "exception_info.h"
#include "probe.h"

#ifdef METHOD_COUNT
#include "retranslate.h"
#endif



#ifdef VIRTUAL_PROFILE
#include "virtual_call_profile.h"
#endif 

#endif /* TRANSLATOR */

extern void initialiseLaTTe(void);

extern char* engine_name;
extern char* engine_version;
extern char* java_version;

extern char* realClassPath;
extern int threadStackSize;
extern size_t gc_heap_limit;
extern int flag_verify;
extern int flag_gc;
extern int flag_classload;
extern int flag_jit;
extern size_t gc_heap_allocation_size;

static int options(char**);
static void usage(void);
static size_t parseSize(char*);

#define	CLASSPATH1	"LATTE_CLASSPATH"
#define	CLASSPATH2	"CLASSPATH"

#if defined(TRANSLATOR) && defined(INTERPRETER)
char*    engine_name = "mixed";
#elif defined(TRANSLATOR)
char*    engine_name = "jit";
#elif defined(INTERPRETER)
char*    engine_name = "intrp";
#else
#error execution engine not specified
#endif

char*    engine_version = VMVERSION;

#define CDBG( s )   

#ifdef BYTECODE_PROFILE
void Pbc_print_profile();
#endif // BYTECODE_PROFILE

#ifdef NEW_COUNT_RISC
void PrintRISCInstrProfile();
#endif // NEW_COUNT_RISC

#ifdef INVOKE_STATUS
void print_invoke_status();
#endif /* INVOKE_STATUS */

// hacker modified for gctime flag
struct timeval main_start_time;
extern struct timeval main_start_time;
extern unsigned long long gctime_total;
extern unsigned long long translation_total;
void print_time(void);

void print_allocate_stats();
void print_virtual_call_translation_statistics();

/*
 * MAIN
 */
int
main(int argc, char* argv[])
{
    HArrayOfObject* args;
    Hjava_lang_String** str;
    int i;
    int farg;
    char* cp;

    // hacker modified for gctime flag
    gettimeofday(&main_start_time, NULL);

    /* Initialize explicitly manager memory. */
    gc_fixed_init();


// The below should be before initialiseLaTTe()...
#ifdef BYTECODE_PROFILE
    atexit( Pbc_print_profile );
#ifdef OPTIM_CMP
#error "**** OPTIM_CMP flag make BYTECODE_PROFILE incorrect ****\n" 
#endif
    // turn off every possible optimization. 
    The_Use_Method_Inline_Option = 0; 
    The_Use_Regionopt_Option = 0; 
    The_Use_Loopopt_Option = 0; 
#endif BYTECODE_PROFILE

    /* Get classpath from environment */
    cp = getenv(CLASSPATH1);
    if (cp == 0) {
        cp = getenv(CLASSPATH2);
        if (cp == 0) {
#if defined(DEFAULT_CLASSPATH)
            cp = DEFAULT_CLASSPATH;
#else
            fprintf(stderr, "CLASSPATH is not set!\n");
            exit(1);
#endif
        }
    }
    realClassPath = cp;

    /* Process program options */
    farg = options(argv);

    /* Get the class name to start with */
    if (argv[farg] == 0) {
        usage();
        exit(1);
    }

    CDBG( num_of_executed_instrs = 0; );

#ifdef TRANSLATOR
    if (flag_call_stat) {
        atexit( print_call_stat );
    }
#endif /* TRANSLATOR */

    /* Initialise */
    initialiseLaTTe();

    /* Build an array of strings as the arguments */
    args = (HArrayOfObject*)AllocObjectArray(argc-(farg+1), "Ljava/lang/String");

    /* Build each string and put into the array */
    str = (Hjava_lang_String**)unhand(args)->body;
    for (i = farg+1; i < argc; i++) {
        str[i-(farg+1)] = makeJavaString(argv[i], strlen(argv[i]));
    }

#ifdef INVOKE_STATUS
    atexit( print_invoke_status );
#endif /* INVOKE_STATUS */
    if (flag_time)
      atexit(print_time);

#ifdef VIRTUAL_PROFILE
    atexit(print_virtual_call_info);
#endif

#ifdef EXAMINE_EXCEPTION
    atexit(print_athrow_info);
#endif

#ifdef NEW_COUNT_RISC
    atexit( PrintRISCInstrProfile );
#endif // NEW_COUNT_RISC

    /* Kick it */
    do_execute_java_main_method(argv[farg], args);

    THREAD_EXIT();
    /* This should never return */
    exit(1);
    return(1);
}

/*
 * Process program's flags.
 */
static
int
options(char** argv)
{
    int i;
    int sz;
    userProperty* prop;

    for (i = 1; argv[i] != 0; i++) {
        if (argv[i][0] != '-') {
            break;
        }
        
        if (strcmp(argv[i], "-help") == 0) {
            usage();
            exit(0);
        }
        else if (strcmp(argv[i], "-version") == 0) {
            fprintf(stderr, "LaTTe Virtual Machine\n");
            fprintf(stderr, "Copyright (C) 1999 MASS Laboratory, Seoul National University\n");
            fprintf(stderr, "Engine: %s   Version: %s   Java Version: %s\n", engine_name, engine_version, java_version);
            exit(0);
        }
        else if (strcmp(argv[i], "-classpath") == 0) {
            i++;
            if (argv[i] == 0) {
                fprintf(stderr, "Error: No path found for -classpath option.\n");
                exit(1);
            }
            realClassPath = argv[i];
        }
        else if (strcmp(argv[i], "-ss") == 0) {
            i++;
            if (argv[i] == 0) {
                fprintf(stderr, "Error: No stack size found for -ss option.\n");
                exit(1);
            }
            sz = parseSize(argv[i]);
            if (sz < THREADSTACKSIZE) {
                fprintf(stderr, "Warning: Attempt to set stack size smaller than %d - ignored.\n", THREADSTACKSIZE);
            }
            else {
                threadStackSize = sz;
            }
        }
        else if (strcmp(argv[i], "-mx") == 0) {
            i++;
            if (argv[i] == 0) {
                fprintf(stderr, "Error: No heap size found for -mx option.\n");
                exit(1);
            }
            gc_heap_limit = parseSize(argv[i]);
        }
        else if (strcmp(argv[i], "-ms") == 0) {
            i++;
            if (argv[i] == 0) {
                fprintf(stderr, "Error: No heap size found for -ms option.\n");
                exit(1);
            }
            gc_heap_allocation_size = parseSize(argv[i]);
        }
#ifdef METHOD_COUNT

        else if (strcmp(argv[i], "-adapt") == 0) {
            flag_adapt = 1;
        }

#ifdef TRANSLATOR
        else if (strcmp(argv[i], "verbosetrstats") == 0
		 || strcmp(argv[i], "-vtrstats") == 0) {
            atexit(print_trstats);
        }
#endif /* TRANSLATOR */

        else if (strcmp(argv[i], "-count") == 0) {
            i++;
            if (argv[i] == 0) {
                fprintf(stderr,
                        "Error: No count for -count.\n");
                exit(1);
            }
            The_Retranslation_Threshold = parseSize(argv[i]);
        }
        else if (strcmp(argv[i], "-max_count") == 0) {
            i++;
            if (argv[i] == 0) {
                fprintf(stderr,
                        "Error: No count for -max_count.\n");
                exit(1);
            }
            The_Max_Retranslation_Threshold = parseSize(argv[i]);
        }
#endif /* METHOD_COUNT */
        else if (strcmp(argv[i], "-verify") == 0) {
            flag_verify = 3;
        }
        else if (strcmp(argv[i], "-verifyremote") == 0) {
            flag_verify = 2;
        }
        else if (strcmp(argv[i], "-noverify") == 0) {
            flag_verify = 0;
        }
        else if (strcmp(argv[i], "-verbosegc") == 0
		 || strcmp(argv[i], "-vgc") == 0) {
            flag_gc = 1;
        }
        else if (strcmp(argv[i], "-verbosejit") == 0
		 || strcmp(argv[i], "-vjit") == 0) {
            flag_jit = 1;
        }
        else if (strcmp(argv[i], "-verboseloader") == 0
		 || strcmp(argv[i], "-vloader") == 0) {
            flag_classload = 1;
        }
        else if (argv[i][1] ==  'D') {
            /* Set a property */
            prop = malloc(sizeof(userProperty));
            assert(prop != 0);
            prop->next = userProperties;
            userProperties = prop;
            for (sz = 2; argv[i][sz] != 0; sz++) {
                if (argv[i][sz] == '=') {
                    argv[i][sz] = 0;
                    sz++;
                    break;
                }
            }
            prop->key = &argv[i][2];
            prop->value = &argv[i][sz];
        }
        /* The following options are not supported and will be
         * ignored for compatibility purposes.
         */
        else if (strcmp(argv[i], "-noasyncgc") == 0 ||
                 strcmp(argv[i], "-cs") == 0 ||
                 strcmp(argv[i], "-debug") == 0 ||
                 strcmp(argv[i], "-checksource") == 0) {
        }
        else if (strcmp(argv[i], "-ms") == 0 ||
                 strcmp(argv[i], "-oss") == 0) {
            i++;
        
// #ifdef JP_OPTIM_INVOKE_DBG
//         } else if (strcmp(argv[i], "-vi") == 0) {
//             i++;
//             if (argv[i] == 0) {
//                 fprintf(stderr, "Error: No method number given for -sched option.\n");
//                 exit(1);
//             }
//             vi_limit = atoi(argv[i]);
// #endif
        }
        else if (strcmp(argv[i], "-vtm") == 0
		 || strcmp(argv[i], "-verbosetm") == 0) {
            flag_translated_method = 1;
        }
        else if (strcmp(argv[i], "-verbosevcg") == 0
		 || strcmp(argv[i], "-vvcg") == 0) {
            flag_vcg = 1;
        }
	else if (strcmp(argv[i], "-vasm") == 0) {
	    flag_code = 1;
	}
        else if (strcmp(argv[i], "-verboseexception") == 0
		 || strcmp(argv[i], "-vexception") == 0) {
            flag_exception = 1;
        }
        else if (strcmp(argv[i], "-verbosecm") == 0
		 || strcmp(argv[i], "-vcm") == 0) {
            flag_called_method = 1;
        }
        else if (strcmp(argv[i], "-newjoin") == 0) {
            flag_old_join = 0;
        }
        else if (strcmp(argv[i], "-verbosetime") == 0
		 || strcmp(argv[i], "-vtime") == 0) {
            flag_time = 1;
        }
        else if (strcmp(argv[i], "-call_stat") == 0) {
            flag_call_stat = 1;
	}
        else if (strcmp(argv[i], "-nocse") == 0 ) {
            flag_no_cse = 1;
        }
        else if (strcmp(argv[i], "-noregionopt") == 0 ) {
            The_Use_Regionopt_Option = 0; 
        }
        else if (strcmp(argv[i], "-noregionoptnum") == 0 ) {
            flag_no_regionopt = 0;
            i++;
            flag_no_regionopt_num = atoi( argv[i] );
        }
        else if (strcmp(argv[i], "-regionoptnum") == 0 ) {
            flag_no_regionopt = 1;
            i++;
            flag_regionopt_num = atoi( argv[i] );
        }
        else if (strcmp(argv[i], "-nodce") == 0 ) {
            flag_no_dce = 1;
        }
        else if (strcmp(argv[i], "-dcenum") == 0 ) {
            flag_no_dce = 1;
            i++;
            flag_dce_num = atoi( argv[i] );
        }
        else if (strcmp(argv[i], "-noloopopt") == 0) {
            The_Use_Loopopt_Option = 0; 
        } 
        else if (strcmp(argv[i], "-loopoptnum") == 0) 
          /* loop opt only for the num-th method */
        {
            The_Use_Loopopt_Option = 0;
            i++;
            flag_loopopt_num = atoi(argv[i]);
        }
        else if (strcmp(argv[i], "-nolooppeeling") == 0) {
            flag_looppeeling = 0;
        } 
        else if (strcmp(argv[i], "-nobackwardsweep") == 0 ) {
            flag_no_backward_sweep = 1;
        }
        else if (strcmp(argv[i], "-noliveanalysis") == 0 ) {
            flag_no_live_analysis = 1;
        }
        else if (strcmp(argv[i], "-nocopycoalescing") == 0 ) {
            flag_no_copy_coalescing = 1;
        }
        else if (strcmp(argv[i], "-inlining") == 0) {
            The_Use_Method_Inline_Option = 1;   // This is the default now.
        }
        else if (strcmp(argv[i], "-noinlining") == 0) {
            The_Use_Method_Inline_Option = 0;
        }
        else if (strcmp(argv[i], "-nocheckcast") == 0) {
            flag_opt_checkcast = 0;
        }
        else if (strcmp(argv[i], "-noinstanceof") == 0) {
            flag_opt_instanceof = 0;
        }

#ifdef TRANSLATOR
        else if (strcmp(argv[i], "-in_depth") == 0 ) {
            i++;
            The_Method_Inlining_Max_Depth = atoi( argv[i] );
        }
        else if (strcmp(argv[i], "-in_size") == 0 ) {
            i++;
            The_Method_Inlining_Max_Size = atoi( argv[i] );
        }
#endif /* TRANSLATOR */

#ifdef TYPE_ANALYSIS
        else if (strcmp(argv[i], "-nota") == 0 ) {
            The_Use_TypeAnalysis_Flag = 0;
        }
#endif

#ifdef CUSTOMIZATION
        else if (strcmp(argv[i], "-nocustom") == 0 ){
            The_Use_Customization_Flag = 0;
        }
        else if (strcmp(argv[i], "-special") == 0 ){
            The_Use_Specialization_Flag = 1;
        }
#endif
        else if (strcmp(argv[i], "-Obase") == 0) {
            The_Use_Regionopt_Option = 0;
            The_Use_Method_Inline_Option = 0;
            The_Use_Loopopt_Option = 0;

#ifdef TYPE_ANALYSIS
            The_Use_TypeAnalysis_Flag = 0;
#endif

#ifdef CUSTOMIZATION
            The_Use_Customization_Flag = 0;
            The_Use_Specialization_Flag = 0;
#endif

#ifdef METHOD_COUNT
            flag_adapt = 0;
#endif
        } else if (strcmp(argv[i], "-Oopt") == 0) {
            The_Use_Regionopt_Option = 1;
            The_Use_Method_Inline_Option = 1;
            The_Use_Loopopt_Option = 1;

#ifdef TYPE_ANALYSIS
            The_Use_TypeAnalysis_Flag = 1;
#endif

#ifdef CUSTOMIZATION
            The_Use_Customization_Flag = 1;
            The_Use_Specialization_Flag = 0;
#endif

#ifdef METHOD_COUNT
            flag_adapt = 0;
#endif
        } else if (strcmp(argv[i], "-Oadapt") == 0) {
            The_Use_Regionopt_Option = 1;
            The_Use_Method_Inline_Option = 1;
            The_Use_Loopopt_Option = 1;

#ifdef TYPE_ANALYSIS
            The_Use_TypeAnalysis_Flag = 1;
#endif

#ifdef CUSTOMIZATION
            The_Use_Customization_Flag = 1;
            The_Use_Specialization_Flag = 0;
#endif

#ifdef METHOD_COUNT
            flag_adapt = 1;
#endif
        }

#if defined(INTERPRETER) && defined(TRANSLATOR)
        else if ( strcmp(argv[i], "-nointrp" ) == 0 ) {
            The_Use_Interpreter_Flag = 0;
        }
#endif /* defined(INTERPRETER) && defined(TRANSLATOR) */

        else {
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
        }
    }

    /* Return first no-flag argument */
    return (i);
}

/*
 * Print usage message.
 */
static
void
usage(void)
{
    fprintf(stderr, "usage: " VMNAME " [options] class\n");
    fprintf(stderr, "Options are:\n");
    fprintf(stderr, "   -help                 Print this message\n");
    fprintf(stderr, "   -version              Print version number\n");
    fprintf(stderr, "   -ss <size>            Maximum native stack size\n");
    fprintf(stderr, "   -mx <size>            Maximum heap size\n");
    fprintf(stderr, "   -ms <size>            Initial heap size\n");
    fprintf(stderr, "   -classpath <path>     Set classpath\n");
    fprintf(stderr, "   -D<property>=<value>  Set a property\n");
    fprintf(stderr, "   -verbosegc, -vgc      Print message during garbage collection\n");
    fprintf(stderr, "   -verboseloader, -vloader\n"
	            "                         Print message while loading classes\n");

#ifdef TRANSLATOR
    fprintf(stderr, "   -verbosejit, -vjit    Print message during JIT code generation\n");
    fprintf(stderr, "   -verbosetm, -vtm      Prints translated methods\n");
    fprintf(stderr, "   -verbosecm, -vcm      Prints called methods\n");
    fprintf(stderr, "   -verbosevcg, -vvcg    Prints vcg file for each methods\n");
    fprintf(stderr, "   -verboseasm, -vasm    Prints asm file for each methods\n");
#endif /* TRANSLATOR */

    fprintf(stderr, "   -verboseexception, -vexception\n"
	            "                         Prints generated exception\n");
    fprintf(stderr, "   -verbosetime, -vtime  Prints garbage collection and translation time\n");



#ifdef TRANSLATOR
    fprintf(stderr, "   -Obase                Without any optimization\n");
    fprintf(stderr, "   -Oopt                 Turn on all the classical optimizations\n");
    fprintf(stderr, "   -Oadapt               -Oopt + turn on adaptive compilation\n");

#ifdef METHOD_COUNT
    fprintf(stderr, "   -verbosetrstats, -vtrstats\n"
	            "                         Print translation statistics\n");
    fprintf(stderr, "   -adapt                Retranslate based on method run count\n");
    fprintf(stderr, "   -count <count>        Retranslate after method runs <count> times\n");
#endif /* METHOD_COUNT */

#if defined(INTERPRETER) && defined(TRANSLATOR)
    fprintf(stderr, "   -nointrp              Don't use interpreter\n");
#endif

    fprintf(stderr, "   -nocse                Disable common subexpression elimination\n");
    fprintf(stderr, "   -noloopopt            Disable loop invariant code motion\n");
    fprintf(stderr, "   -noinlining           Disable method inlining\n");

#ifdef CUSTOMIZATION
    fprintf(stderr, "   -nocustom             Disable customization\n");
    fprintf(stderr, "   -nospecial            Disable specialization\n");
#endif

#endif /* TRANSLATOR */
}

void
throwOutOfMemory ()
{
	Hjava_lang_Object* err;

	err = OutOfMemoryError;
	if (err != NULL) {
		throwException(err);
	}
	fprintf (stderr, "(Insufficient memory)\n");
	exit (-1);
}

static
size_t
parseSize(char* arg)
{
	size_t sz;
	char* narg;

	sz = strtol(arg, &narg, 0);
	switch (narg[0]) {
	case 'b': case 'B':
		break;

	case 'k': case 'K':
		sz *= 1024;
		break;

	case 'm': case 'M':
		sz *= 1024 * 1024;
		break;
	}

	return (sz);
}

void
print_time(void) 
{
    // hacker modified for gctime flag
    unsigned long long total_execution_time;
    struct timeval main_finish_time;
        
    gettimeofday(&main_finish_time, NULL);
    total_execution_time = (main_finish_time.tv_sec 
                            - main_start_time.tv_sec) 
        * 1000 + (main_finish_time.tv_usec - 
                  main_start_time.tv_usec) / 1000;
    fprintf(stderr, "Total Execution Time : %.3f sec.\n", 
	    total_execution_time / 1000.0);

#ifdef TRANSLATOR
    fprintf(stderr, "Total Translation Time : %.3f sec (%.2f%%).\n", 
	    translation_total / 1000.0, 
	    translation_total / (total_execution_time / 100.0));
#endif /* TRANSLATOR */

    /* Print garbage collection time. */
    fprintf(stderr,
	    "Total Garbage Collection Time : %.3f sec (%.2f%%).\n", 
	    gc_stats.total_gc,
	    gc_stats.total_gc / (total_execution_time / (100.0 * 1000.0)));
    fprintf(stderr,
	    "\tMark  : %.3f sec (%.2f%%).\n",
	    gc_stats.total_mark,
	    100.0 * gc_stats.total_mark / gc_stats.total_gc);
    fprintf(stderr,
	    "\tSweep : %.3f sec (%.2f%%).\n",
	    gc_stats.total_sweep,
	    100.0 * gc_stats.total_sweep / gc_stats.total_gc);
    fprintf(stderr,
	    "\t\tSort : %.3f sec (%.2f%%).\n",
	    gc_stats.total_sort,
	    100.0 * gc_stats.total_sort / gc_stats.total_sweep);

#ifdef __STAT_NEW_TIME__
    fprintf(stderr, "Total NEW Time : %llu mili sec\n",
           (stat_newPrimArray_time + stat_newObject_time
          +stat_newRefArray_time + stat_newMultiArray_time) / 1000
          );
#endif __STAT_NEW_TIME__
}
