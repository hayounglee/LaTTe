
#ifndef __DEFAULT_H__
#define __DEFAULT_H__

#include <assert.h>

#if !defined(__cplusplus) && !defined(TRUE) && !defined(FALSE)
typedef enum { false = 0, true = 1 } bool;
#endif

#ifndef NULL
#define NULL ((void *)(0))
#endif  NULL

#endif  __DEFAULT_H__
