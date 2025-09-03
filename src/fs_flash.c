#include "fs_def.h"
#include "fs_std.h"
#include "fs_emulator.h"
#include "fs_crypt.h"
#include "fs_flash.h"

/*
 * Магическое число заголовка (fldr)
 */
#define FLASH_HEADER_MAGIC 0x666C6472

/*
 * magic: Идентификатор (0x666C6472)
 * sectors: Массив секторов
 * (148 + 12 * enum)
 * mode: Режим работы
 */
typedef struct
{
  U32 magic;
  FLASH_SECTOR_TYPE sectors[FLASH_SECTORS_COUNT];
  FLASH_MODE mode;
  U32 crc32;
} FLASH_HEADER_TYPE;

/*
 * Адреса начала секторов + адрес конца
 * G_SECTORS_ADDRESS[0U]: Код системы (16 кб)
 * G_SECTORS_ADDRESS[1U]: Метаданные flash-драйвера (16 кб)
 * G_SECTORS_ADDRESS[2U]: Пользовательские данные (FTL драйвер) (16 кб)
 * G_SECTORS_ADDRESS[3U]: Пользовательские данные (FTL драйвер) (16 кб)
 * G_SECTORS_ADDRESS[4U]: Пользовательские данные (FTL драйвер) (64 кб)
 * G_SECTORS_ADDRESS[5U+]: Пользовательские данные (FTL драйвер) (128 кб)
 */
const FLASH_ADDRESS G_SECTORS_ADDRESS[FLASH_SECTORS_COUNT + 1U] =
{
  0x08000000, 0x08004000, 0x08008000,
  0x0800C000, 0x08010000, 0x08020000,
  0x08040000, 0x08060000, 0x08080000,
  0x080A0000, 0x080C0000, 0x080E0000,
  0x08100000
};

/*
 * Стандартные права доступа к секторам
 */
const FLASH_ACCESS G_SECTORS_ACCESS[FLASH_SECTORS_COUNT] =
{
  FLASH_ACCESS_SUPERVISOR, FLASH_ACCESS_READ_ONLY,  FLASH_ACCESS_READ_WRITE,
  FLASH_ACCESS_READ_WRITE, FLASH_ACCESS_READ_WRITE, FLASH_ACCESS_READ_WRITE,
  FLASH_ACCESS_READ_WRITE, FLASH_ACCESS_READ_WRITE, FLASH_ACCESS_READ_WRITE,
  FLASH_ACCESS_READ_WRITE, FLASH_ACCESS_READ_WRITE, FLASH_ACCESS_READ_WRITE,
};


/*
 * Служебные данные флеш секторов
 */
static FLASH_HEADER_TYPE g_flash_header =
(FLASH_HEADER_TYPE){
  .mode = FLASH_MODE_SUPERVISOR
};


/*
 * ДОСТУП К СЕКТОРУ: (!!! ОПТИМИЗИРОВАТЬ !!!)
 *   SECTOR_ID: Номер сектора
 *   REQUIRED_ACCESS: Требуемые права
 *   return_code: Статус операции
 *     NO_ERROR: Доступ разрешен
 *     ACCESS_DENIED: Доступ запрещен
 *     INVALID_PARAM: Неверный номер сектора
 */
void FLASH_SECTOR_VERIFY(
/* IN  */ const FLASH_SECTOR_ID SECTOR_ID,
/* IN  */ const FLASH_ACCESS REQUIRED_ACCESS,
/* OUT */ RETURN_CODE * return_code)
{
  if(SECTOR_ID >= FLASH_SECTORS_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  if(FLASH_MODE_SUPERVISOR == g_flash_header.mode)
  {
    *return_code = NO_ERROR;
    return;
  }

  if(REQUIRED_ACCESS > g_flash_header.sectors[SECTOR_ID].permission)
  {
    *return_code = ACCESS_DENIED;
    return;
  }

  if((FLASH_ACCESS_SUPERVISOR == REQUIRED_ACCESS)
  && (FLASH_MODE_USER == g_flash_header.mode))
  {
    *return_code = ACCESS_DENIED;
    return;
  }

  *return_code = NO_ERROR;
}



/*
 * УТВЕРДИТЬ ИЗМЕНЕНИЯ В СЕКТОРЕ:
 *   SECTOR_ID: Номер сектора
 *   return_code: Статус операции
 *     NO_ERROR: Данные в секторе утверждены
 *     INVALID_PARAM: Неверный номер сектора
 *     ACCESS_DENIED: Требуется режим суперпользователя
 */
void FLASH_SECTOR_ADMIT(
/* IN  */ const FLASH_SECTOR_ID SECTOR_ID,
/* OUT */ RETURN_CODE * return_code)
{
  if(SECTOR_ID >= FLASH_SECTORS_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  if(FLASH_MODE_SUPERVISOR != g_flash_header.mode)
  {
    *return_code = ACCESS_DENIED;
    return;
  }

  HASH_CRC(
    &(g_flash_header.sectors[SECTOR_ID]), sizeof(FLASH_SECTOR_TYPE) - sizeof(U32),
    &(g_flash_header.sectors[SECTOR_ID].crc32)
  );

  *return_code = NO_ERROR;
}

/*
 * ПРОВЕРКА ЦЕЛОСТНОСТИ СЕКТОРА (!!! СДЕЛАТЬ ФОРМАТИРОВАНИЕ СЕКТОРА !!!):
 *   SECTOR_ID: Номер сектора
 *   return_code: Статус операции
 *     NO_ERROR: Сектор корректен
 *     INVALID_PARAM: Неверный номер сектора
 *     OPERATION_FAILED: Сектор не валиден
 */
void FLASH_SECTOR_VALIDATE(
/* IN  */ const FLASH_SECTOR_ID SECTOR_ID,
/* OUT */ RETURN_CODE * return_code)
{
  if(SECTOR_ID >= FLASH_SECTORS_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  U32 m_sector_crc32_calc = 0UL;
  HASH_CRC(
    &(g_flash_header.sectors[SECTOR_ID]), sizeof(FLASH_SECTOR_TYPE) - sizeof(U32),
    &m_sector_crc32_calc
  );
  if(m_sector_crc32_calc != g_flash_header.sectors[SECTOR_ID].crc32)
  {
    *return_code = OPERATION_FAILED;
  }
  else
  {
    *return_code = NO_ERROR;
  }
}

/*
 * УТВЕРДИТЬ ИЗМЕНЕНИЯ В FLASH
 *   return_code: Статус операции
 *     NO_ERROR: Данные в flash утверждены
 *     ACCESS_DENIED: Требуется режим суперпользователя
 *     OPERATION_FAILED: Данные в секторах невалидны
 */
void FLASH_ADMIT(
/* OUT */ RETURN_CODE * return_code)
{
  if(FLASH_MODE_SUPERVISOR != g_flash_header.mode)
  {
    *return_code = ACCESS_DENIED;
    return;
  }

  RETURN_CODE m_validate_error = NO_ERROR;
  for(register U8 i = 0U; i < FLASH_SECTORS_COUNT; i++)
  {
    FLASH_SECTOR_VALIDATE(i, &m_validate_error);
    if(NO_ERROR != m_validate_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }

  HASH_CRC(
    &(g_flash_header), sizeof(FLASH_HEADER_TYPE) - sizeof(U32),
    &(g_flash_header.crc32)
  );

  *return_code = NO_ERROR;
}

/*
 * УТВЕРДИТЬ ИЗМЕНЕНИЯ В FLASH:
 *   return_code: Статус операции
 *     NO_ERROR: Данные в flash корректны
 *     OPERATION_FAILED: Данные в flash невалидны
 */
void FLASH_VALIDATE(
/* OUT */ RETURN_CODE * return_code)
{
  U32 m_flash_crc32_calc = 0Ul;
  HASH_CRC(
    &(g_flash_header), sizeof(FLASH_HEADER_TYPE) - sizeof(U32),
    &m_flash_crc32_calc
  );
  if(m_flash_crc32_calc != g_flash_header.crc32)
  {
    *return_code = OPERATION_FAILED;
  }
  else
  {
    *return_code = NO_ERROR;
  }
}


/*
 * УСТАНОВКА РЕЖИМА РАБОТЫ:
 *   MODE: Режим работы
 *   return_code: Статус операции
 *     NO_ERROR: Режим установлен
 *     OPERATION_FAILED: Сектора невалидны
 */
inline static void FLASH_MODE_SET(
/* IN  */ const FLASH_MODE MODE,
/* OUT */ RETURN_CODE * return_code)
{
  g_flash_header.mode = MODE;
  RETURN_CODE m_validate_error = NO_ERROR;
  for(register U8 i = 0U; i < FLASH_SECTORS_COUNT; i++)
  {
    FLASH_SECTOR_VALIDATE(i, &m_validate_error);
    if(NO_ERROR != m_validate_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }

  HASH_CRC(
    &(g_flash_header), sizeof(FLASH_HEADER_TYPE) - sizeof(U32),
    &(g_flash_header.crc32)
  );

  *return_code = NO_ERROR;
}




void FLASH_INIT(
/* OUT */ RETURN_CODE * return_code)
{
  /* 1. Функцию может вызывать только SUPERVISOR */
  if(FLASH_MODE_SUPERVISOR != g_flash_header.mode)
  {
    *return_code = ACCESS_DENIED;
    return;
  }

  const SIZE32 M_SUPERBLOCK_SIZE =
    sizeof(U32) + sizeof(FLASH_SECTOR_TYPE) * FLASH_SECTORS_COUNT;

  /*
  FLASH_ADDRESS m_address
    = G_SECTORS_ADDRESS[2U] - 1U
    - ((G_SECTORS_ADDRESS[2U] - G_SECTORS_ADDRESS[1U]) % M_SUPERBLOCK_SIZE);

  I8 m_is_find = 0;
  while(!m_is_find)
  {
    m_address = m_address - M_SUPERBLOCK_SIZE;
    if(m_address < G_SECTORS_ADDRESS[1U])
    {
      m_is_find = 1;
    }

    RETURN_CODE m_read_error = NO_ERROR;
    FLASH_READ(
      m_address, M_SUPERBLOCK_SIZE, &g_flash_header, &m_read_error
    );
    if(NO_ERROR != m_read_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }

    if(((U8 *)&g_flash_header)[0U] != 0xFF)
    {
      m_is_find = 1;
    }
  }
  */
  /* 2. Чтение суперблока flash драйвера */
  /* sizeof(magic) + sizeof(sectors) */
  RETURN_CODE m_read_error = NO_ERROR;
  FLASH_READ(
    G_SECTORS_ADDRESS[1U], M_SUPERBLOCK_SIZE, &g_flash_header, &m_read_error
  );
  if(NO_ERROR != m_read_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  RETURN_CODE m_admit_error = NO_ERROR;

  // 3. Проверка магического числа
  if(g_flash_header.magic != FLASH_HEADER_MAGIC)
  {
    /* 3.1. Инициализация при первом запуске */
    g_flash_header.magic = FLASH_HEADER_MAGIC;

    for(register U8 i = 0U; i < FLASH_SECTORS_COUNT; i++)
    {
      g_flash_header.sectors[i] =
      (FLASH_SECTOR_TYPE){
        .pba = G_SECTORS_ADDRESS[i],
        .permission = G_SECTORS_ACCESS[i],
        .wear = 0U
      };
      FLASH_SECTOR_ADMIT(i, &m_admit_error);
      if(NO_ERROR != m_admit_error)
      {
        *return_code = OPERATION_FAILED;
        return;
      }
    }

    /* 3.2. Стирание сектора (готов к записи) */
    for(register U8 i = 1U; i < FLASH_SECTORS_COUNT; i++)
    {
      RETURN_CODE m_erase_error = NO_ERROR;
      FLASH_SECTOR_ERASE(i, &m_erase_error);
      if(NO_ERROR != m_erase_error)
      {
        *return_code = OPERATION_FAILED;
        return;
      }
    }
  }

  /* 4. Переход в пользовательский режим */
  RETURN_CODE m_mode_error = NO_ERROR;
  FLASH_MODE_SET(FLASH_MODE_USER, &m_mode_error);
  if(NO_ERROR != m_mode_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  *return_code = NO_ERROR;
}

void FLASH_FREE(
/* OUT */ RETURN_CODE * return_code)
{
  g_flash_header.mode = FLASH_MODE_SUPERVISOR;

  RETURN_CODE m_erase_error = NO_ERROR;
  FLASH_SECTOR_ERASE(1U, &m_erase_error);
  /* TODO: m_erase_error */

  const SIZE32 M_SUPERBLOCK_SIZE =
    sizeof(U32) + sizeof(FLASH_SECTOR_TYPE) * FLASH_SECTORS_COUNT;
  RETURN_CODE m_write_error = NO_ERROR;
  FLASH_WRITE(
    G_SECTORS_ADDRESS[1U], M_SUPERBLOCK_SIZE, &g_flash_header, &m_write_error
  );
  if(NO_ERROR != m_write_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }
  *return_code = NO_ERROR;
}

void FLASH_SECTOR_BORDERS(
/* IN  */ FLASH_SECTOR_ID SECTOR_ID,
/* OUT */ FLASH_ADDRESS * pba_start,
/* OUT */ FLASH_ADDRESS * pba_end,
/* OUT */ RETURN_CODE * return_code)
{
  if(SECTOR_ID >= FLASH_SECTORS_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  *pba_start = G_SECTORS_ADDRESS[SECTOR_ID];
  *pba_end = G_SECTORS_ADDRESS[SECTOR_ID + 1U] - 1UL;
  *return_code = NO_ERROR;
}

void FLASH_SECTOR_FIND(
/* IN  */ const FLASH_ADDRESS PBA,
/* OUT */ FLASH_SECTOR_ID * sector_id,
/* OUT */ RETURN_CODE * return_code)
{
  if((PBA < G_SECTORS_ADDRESS[0U])
  || (PBA >= G_SECTORS_ADDRESS[FLASH_SECTORS_COUNT]))
  {
    *return_code = INVALID_PARAM;
    return;
  }

  U8 m_low = 0U;
  U8 m_high = FLASH_SECTORS_COUNT - 1U;

  while(m_low <= m_high)
  {
    U8 m_mid = m_low + (m_high - m_low) / 2U;

    if((PBA >= G_SECTORS_ADDRESS[m_mid])
    && (PBA < G_SECTORS_ADDRESS[m_mid + 1U]))
    {
      *sector_id = m_mid;
      *return_code = NO_ERROR;
      return;
    }
    else if(PBA < G_SECTORS_ADDRESS[m_mid])
    {
      if(0U == m_mid)
      {
        break;
      }
      m_high = m_mid - 1U;
    }
    else
    {
      m_low = m_mid + 1U;
    }
  }

  *return_code = OPERATION_FAILED;
}

void FLASH_SECTOR_SELECT(
/* IN  */ const FLASH_SECTOR_ID SECTOR_ID,
/* OUT */ FLASH_SECTOR_TYPE * sector,
/* OUT */ RETURN_CODE * return_code)
{
  if(SECTOR_ID >= FLASH_SECTORS_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  *sector = g_flash_header.sectors[SECTOR_ID];

  *return_code = NO_ERROR;
}



void FLASH_SECTOR_ERASE(
/* IN  */ const FLASH_SECTOR_ID SECTOR_ID,
/* OUT */ RETURN_CODE * return_code)
{
  if(SECTOR_ID >= FLASH_SECTORS_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  RETURN_CODE m_verify_error = NO_ERROR;
  FLASH_SECTOR_VERIFY(SECTOR_ID, FLASH_ACCESS_READ_WRITE, &m_verify_error);
  if(NO_ERROR != m_verify_error)
  {
    *return_code = ACCESS_DENIED;
    return;
  }

  const SIZE32 M_SECTOR_SIZE
    = G_SECTORS_ADDRESS[SECTOR_ID + 1] - G_SECTORS_ADDRESS[SECTOR_ID];
  const FLASH_ADDRESS M_SECTOR_OFFSET
    = G_SECTORS_ADDRESS[SECTOR_ID] - G_SECTORS_ADDRESS[0U];
  STD_MEMSET(M_SECTOR_SIZE, 0xFF, g_flash_mem + M_SECTOR_OFFSET);

  // Увеличиваем счетчик стираний
  g_flash_header.sectors[SECTOR_ID].wear++;

  // Временное повышение прав для обновления метаданных
  FLASH_MODE m_mode_tmp = g_flash_header.mode;
  g_flash_header.mode = FLASH_MODE_SUPERVISOR;

  RETURN_CODE m_sector_admit_error = NO_ERROR;
  FLASH_SECTOR_ADMIT(SECTOR_ID, &m_sector_admit_error);
  if(NO_ERROR != m_sector_admit_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  // Восстановление предыдущего режима
  RETURN_CODE m_mode_error = NO_ERROR;
  FLASH_MODE_SET(m_mode_tmp, &m_mode_error);
  if(NO_ERROR != m_mode_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  *return_code = NO_ERROR;
}



void FLASH_WRITE(
  const FLASH_ADDRESS PBA,
  const SIZE32 SIZE,
  const VOID_PTR DATA,
  RETURN_CODE * return_code)
{
  // 1. Проверка выравнивания
  if((PBA & 0x3U) || ((U32)DATA & 0x3U) || (SIZE & 0x3U))
  {
    *return_code = INVALID_PARAM;
    return;
  }

  // 2. Определение сектора
  FLASH_SECTOR_ID m_sector_id;
  FLASH_SECTOR_FIND(PBA, &m_sector_id, return_code);
  if(NO_ERROR != *return_code)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  // 3. Проверка доступа
  FLASH_ACCESS required_access = FLASH_ACCESS_READ_WRITE;
  if(FLASH_MODE_SUPERVISOR == g_flash_header.mode)
  {
    required_access = FLASH_ACCESS_SUPERVISOR;
  }

  FLASH_SECTOR_VERIFY(m_sector_id, required_access, return_code);
  if(NO_ERROR != *return_code)
  {
    *return_code = ACCESS_DENIED;
    return;
  }

  // 4. Проверка границ сектора
  FLASH_ADDRESS m_pba_start = 0x0U;
  FLASH_ADDRESS m_pba_end   = 0x0U;
  FLASH_SECTOR_BORDERS(m_sector_id, &m_pba_start, &m_pba_end, return_code);
  if((PBA < m_pba_start) || ((PBA + SIZE - 1U) > m_pba_end))
  {
    *return_code = INVALID_PARAM;
    return;
  }

  // 5. Низкоуровневая запись
  const FLASH_ADDRESS M_OFFSET = PBA - G_SECTORS_ADDRESS[0U];
  for(SIZE32 i = M_OFFSET; i < M_OFFSET + SIZE; i++)
  {
    if(*(U8 *)(g_flash_mem + i) != 0xFF)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }
  STD_MEMCPY(SIZE, DATA, g_flash_mem + M_OFFSET);

  *return_code = NO_ERROR;
}

void FLASH_READ(
/* IN  */ const FLASH_ADDRESS PBA,
/* IN  */ const SIZE32 SIZE,
/* OUT */ VOID_PTR data,
/* OUT */ RETURN_CODE * return_code)
{
  // 1. Проверка выравнивания
  if((PBA & 0x3U) || ((U32)data & 0x3U) || (SIZE & 0x3U))
  {
    *return_code = INVALID_PARAM;
    return;
  }

  // 2. Проверка границ памяти
  if((PBA < G_SECTORS_ADDRESS[0U])
  || (PBA + SIZE >= G_SECTORS_ADDRESS[FLASH_SECTORS_COUNT]))
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  // 3. Низкоуровневое чтение
  const FLASH_ADDRESS M_OFFSET = PBA - G_SECTORS_ADDRESS[0U];
  STD_MEMCPY(SIZE, g_flash_mem + M_OFFSET, data);

  *return_code = NO_ERROR;
}
