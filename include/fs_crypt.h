#ifndef __FS_CRYPT_H__
#define __FS_CRYPT_H__

#include "fs_def.h"

/*
 * ШИФРОВАНИЕ XOR:
 *   data: Шифруемые данные
 *   SIZE: Размер данных
 *   ADDRESS: Ключ шифрования (адрес)
 */
void CRYPT_XOR(
/* INOUT */ VOID_PTR data,
/* IN    */ const SIZE32 SIZE,
/* IN    */ const U32 ADDRESS);

/*
 * ХЕШИРОВАНИЕ CRC32:
 *   DATA: Хешируемые данные
 *   SIZE: Размер данных
 *   crc: Хеш-номер
 */
void HASH_CRC(
/* IN  */ const VOID_PTR DATA,
/* IN  */ const SIZE32 SIZE,
/* OUT */ U32 * crc);

#endif /* __FS_CRYPT_H__ */
