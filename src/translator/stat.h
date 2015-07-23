//
// File Name : stat.h
// Author    : Yang, Byung-Sun
//
// This is for extract statistics of the JVM.
//

#ifndef __STAT_H__
#define __STAT_H__

#define VIRTUAL_CALL_TO_PUBLIC       0
#define VIRTUAL_CALL_TO_PROTECTED    1
#define VIRTUAL_CALL_TO_PRIVATE      2
#define VIRTUAL_CALL_TO_ETC          3
#define STATIC_CALL_TO_PUBLIC        4
#define STATIC_CALL_TO_PROTECTED     5
#define STATIC_CALL_TO_PRIVATE       6
#define STATIC_CALL_TO_ETC           7
#define SPECIAL_CALL                 8
#define INTERFACE_CALL               9

void   update_function_call( int call_type, int is_final );

void   print_call_stat();


#ifdef __STAT_NEW_TIME__

extern unsigned long long stat_newObject_time;
extern unsigned long long stat_newRefArray_time;
extern unsigned long long stat_newPrimArray_time;
extern unsigned long long stat_newMultiArray_time;
extern unsigned long long stat_gc_heap_malloc_time;

#endif __STAT_NEW_TIME__

#endif __STAT_H__

