#ifndef __FS_STD_H__
#define __FS_STD_H__

#include "fs_def.h"

void STD_MEMCPY(
/* IN  */ const SIZE32 SIZE,
/* IN  */ const VOID_PTR SRC,
/* OUT */ VOID_PTR dest);

void STD_MEMSET(
/* IN  */ const SIZE32 SIZE,
/* IN  */ const U8 VALUE,
/* OUT */ VOID_PTR dest);

void STD_STRCMP(
/* IN  */ const CHAR * FIRST,
/* IN  */ const CHAR * SECOND,
/* OUT */ I32 * result);

void STD_STRNCPY(
/* IN  */ const SIZE32 SIZE,
/* IN  */ const CHAR * SRC,
/* OUT */ CHAR * dest);

#endif
