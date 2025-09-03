#ifndef __FS_EMULATOR_H__
#define __FS_EMULATOR_H__

#include "fs_def.h"

/*
 * Размер flash памяти 1мб
 */
#define FLASH_SIZE (1024 * 1024)

extern I16  g_flash_fd;
extern U8 * g_flash_mem;

void EMULATOR_INIT(
/* IN  */ const CHAR * FLASH_NAME,
/* OUT */ RETURN_CODE * return_code);

void EMULATOR_FREE(void);

#endif
