#include "config.h"
#include "stat.h"
#include <stdio.h>

#ifdef __STAT_NEW_TIME__
unsigned long long stat_newObject_time = 0;
unsigned long long stat_newRefArray_time = 0;
unsigned long long stat_newPrimArray_time = 0;
unsigned long long stat_newMultiArray_time = 0;
unsigned long long stat_gc_heap_malloc_time = 0;
#endif __STAT_NEW_TIME__

static unsigned long long virtual_call_to_public = 0;
static unsigned long long virtual_call_to_protected = 0;
static unsigned long long virtual_call_to_private = 0;
static unsigned long long virtual_call_to_etc = 0;
static unsigned long long static_call_to_public = 1;
static unsigned long long static_call_to_protected = 0;
static unsigned long long static_call_to_private = 0;
static unsigned long long static_call_to_etc = 0;
static unsigned long long special_call = 0;
static unsigned long long interface_call = 0;
static unsigned long long final_call = 0;

void
print_call_stat()
{
    printf( "Virtual calls to public = %llu\n", virtual_call_to_public );
    printf( "Virtual calls to protected = %llu\n", virtual_call_to_protected );
    printf( "Virtual calls to private = %llu\n", virtual_call_to_private );
    printf( "Virtual calls to etc = %llu\n", virtual_call_to_etc );
    printf( "Static calls to public = %llu\n", static_call_to_public );
    printf( "Static calls to protected = %llu\n", static_call_to_protected );
    printf( "Static calls to private = %llu\n", static_call_to_private );
    printf( "Static calls to etc = %llu\n", static_call_to_etc );
    printf( "Special calls = %llu\n", special_call );
    printf( "Interface calls = %llu\n", interface_call );
    printf( "Final calls = %llu\n", final_call );
}

void
update_function_call( int call_type, int is_final )
{
    switch (call_type) {
      case VIRTUAL_CALL_TO_PUBLIC:
        virtual_call_to_public++;
        break;

      case VIRTUAL_CALL_TO_PROTECTED:
        virtual_call_to_protected++;
        break;

      case VIRTUAL_CALL_TO_PRIVATE:
        virtual_call_to_private++;
        break;

      case VIRTUAL_CALL_TO_ETC:
        virtual_call_to_etc++;
        break;

      case STATIC_CALL_TO_PUBLIC:
        static_call_to_public++;
        break;

      case STATIC_CALL_TO_PROTECTED:
        static_call_to_protected++;
        break;

      case STATIC_CALL_TO_PRIVATE:
        static_call_to_private++;
        break;

      case STATIC_CALL_TO_ETC:
        static_call_to_etc++;
        break;

      case SPECIAL_CALL:
        special_call++;
        break;

      case INTERFACE_CALL:
        interface_call++;
        break;

      default:
        fprintf( stderr, "Strange call type!!!\n" );
        exit( 1 );
    }

    if (is_final) {
        final_call++;
    }
}






