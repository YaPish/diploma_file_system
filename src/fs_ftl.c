#include "fs_def.h"
#include "fs_std.h"
#include "fs_crypt.h"
#include "fs_flash.h"
#include "fs_ftl.h"

/*
 * Размер одного логического блока
 */
#define FTL_BLOCK_SIZE 256U

/*
 * Количество логических блоков (Сектор 1 и 2 не включены (32 кб))
 */
#define FTL_BLOCKS_COUNT 3968U


/*
 * FTL_FLAG_VALID: Блок содержит актуальные данные
 * FTL_FLAG_DIRTY: Блок устарел (данные должны перенестись в новый блок)
 * FTL_FLAG_FREE: Блок свободен для записи
 */
typedef enum
{
  FTL_FLAG_VALID = 0x1,
  FTL_FLAG_DIRTY = 0x2,
  FTL_FLAG_FREE  = 0x3
} FTL_FLAG;

/*
 * ФИЗИЧЕСКИЙ БЛОК:
 *   flag : Статус состояния
 *   permission: Права доступа к блоку
 *   lbi: Логический адрес (4096 блоков * 256 байт = 1МБ)
 *   crc32: CRC32 хеш (32 бита)
 */
typedef struct __packed
{
  FTL_FLAG flag  : 2;  // 3 флага состояния
  U16 lbi        : 12; // 4096 блоков по 256 байт = 1МБ
  U8 reserved    : 2;  // 2 бита резерв
  U32 crc32      : 32; // 32 бита хеш
} FTL_BLOCK_TYPE;

/*
 * table: Массив физических блоков
 *   0-63: SECTOR 2 (16 кб) (64 blocks)
 *   64-127: SECTOR 3 (16 кб) (64 blocks)
 *   128-383: SECTOR 4 (64 кб) (256 blocks)
 *   384-895: SECTOR 5 (128 кб) (512 blocks)
 * (flash: 23812 байт (94 blocks))
 * mode: Режим работы
 * pba: Физический адрес начала доступной памяти
 */
typedef struct
{
  FTL_BLOCK_TYPE table[FTL_BLOCKS_COUNT];
  FTL_MODE mode;
  FLASH_ADDRESS pba;
} FTL_HEADER_TYPE;

/*
 * Служебные данные FTL
 */
static FTL_HEADER_TYPE g_ftl_header =
(FTL_HEADER_TYPE){
  .mode = FTL_MODE_SUPERVISOR
};



/*
 * ВЫДЕЛИТЬ БЛОК:
 *   pbi: Номер физического блока
 *   return_code: Статус операции
 *     NO_ERROR: Блок выделен
 *     OPERATION_FAILED: Не найден свободный блок
 */
void FTL_BLOCK_ALLOCATE(
/* OUT */ FTL_INDEX * pbi,
/* OUT */ RETURN_CODE * return_code)
{
  for(register FTL_INDEX i = 0U; i < FTL_BLOCKS_COUNT; i++)
  {
    if(g_ftl_header.table[i].flag == FTL_FLAG_FREE)
    {
      *pbi = i;
      *return_code = NO_ERROR;
      return;
    }
  }
  *return_code = OPERATION_FAILED;
}

/*
 * ПОЛУЧИТЬ ФИЗ. БЛОК ПО ЛОГИЧЕСКОМУ НОМЕРУ:
 *   LBI: Номер логического блока
 *   pbi: Номер физического блока
 *   return_code: Статус операции
 *     NO_ERROR: Найден физический блок с присвоенным логическим номером
 *     INVALID_PARAM: Номер логического блока выходит за границы
 *     NO_ACTION: Блок устаревший
 *     OPERATION_FAILED: Блок не найден
 */
void FTL_BLOCK_GET(
/* IN  */ const FTL_INDEX LBI,
/* OUT */ FTL_INDEX * pbi,
/* OUT */ RETURN_CODE * return_code)
{
  if(LBI >= FTL_BLOCKS_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  for(register FTL_INDEX i = 0U; i < FTL_BLOCKS_COUNT; i++)
  {
    if(g_ftl_header.table[i].lbi == LBI)
    {
      *pbi = i;
      if(g_ftl_header.table[i].flag == FTL_FLAG_VALID)
      {
        *return_code = NO_ERROR;
      }
      if(g_ftl_header.table[i].flag == FTL_FLAG_DIRTY)
      {
        *return_code = NO_ACTION;
      }
      return;
    }
  }
  *return_code = OPERATION_FAILED;
}

/*
 * ЗАПИСАТЬ БЛОК:
 *   LBI: Номер логического блока
 *   DATA: Блок данных (250 байт)
 *   return_code: Статус операции
 *     NO_ERROR: Успешная запись
 *     OPERATION_FAILED: Невозможно записать данные в память
 */
void FTL_WRITE_BLOCK(
/* IN  */ const FTL_INDEX LBI,
/* IN  */ const VOID_PTR DATA,
/* OUT */ RETURN_CODE * return_code)
{
  /* 1. Найти текущий физический блок (если есть) */
  FTL_INDEX m_old_pbi;
  RETURN_CODE m_get_error = NO_ERROR;
  FTL_BLOCK_GET(LBI, &m_old_pbi, &m_get_error);

  /* 2. Выделить новый физический блок */
  FTL_INDEX m_new_pbi;
  RETURN_CODE m_alloc_error = NO_ERROR;
  FTL_BLOCK_ALLOCATE(&m_new_pbi, &m_alloc_error);
  if(NO_ERROR != m_alloc_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  /* 3. Подготовить данные */
  U8 m_block[FTL_BLOCK_SIZE];
  U8 m_data[FTL_BLOCK_SIZE - sizeof(FTL_BLOCK_TYPE)];
  STD_MEMCPY(FTL_BLOCK_SIZE - sizeof(FTL_BLOCK_TYPE), DATA, m_data);

  // 4.1. Шифрование данных
  FLASH_ADDRESS m_new_pba = m_new_pbi * FTL_BLOCK_SIZE + g_ftl_header.pba;
  //CRYPT_XOR(m_data, FTL_BLOCK_SIZE - sizeof(FTL_BLOCK_TYPE), m_new_pba);

  // 4.2. Вычисление CRC
  U32 m_crc32 = 0U;
  HASH_CRC(m_data, FTL_BLOCK_SIZE - sizeof(FTL_BLOCK_TYPE), &m_crc32);

  // 4.3. Формирование блока: метаданные + данные
  FTL_BLOCK_TYPE m_meta =
  (FTL_BLOCK_TYPE){
    .flag = FTL_FLAG_VALID,
    .lbi = LBI,
    .crc32 = m_crc32
  };

  // Копируем метаданные
  STD_MEMCPY(sizeof(FTL_BLOCK_TYPE), &m_meta, m_block);
  STD_MEMCPY(
    FTL_BLOCK_SIZE - sizeof(FTL_BLOCK_TYPE), m_data,
    m_block + sizeof(FTL_BLOCK_TYPE)
  );

  // 5. Записать во flash
  RETURN_CODE m_write_error = NO_ERROR;
  FLASH_WRITE(m_new_pba, FTL_BLOCK_SIZE, m_block, &m_write_error);
  if(NO_ERROR != m_write_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  // 6. Обновить таблицу FTL
  g_ftl_header.table[m_new_pbi] = m_meta;
  if(NO_ERROR == m_get_error)
  {
    g_ftl_header.table[m_old_pbi].flag = FTL_FLAG_DIRTY;
  }

  *return_code = NO_ERROR;
}

/*
 * ПРОЧИТАТЬ БЛОК:
 *   LBI: Номер логического блока
 *   data: Блок данных (250 байт)
 *   return_code: Статус операции
 *     NO_ERROR: Блок успешно прочитан
 *     NO_ACTION: Блока не существует или не доступен
 *     OPERATION_FAILED: Чтение не выполнено или данные повреждены
 */
void FTL_READ_BLOCK(
/* IN  */ const U32 LBI,
/* OUT */ VOID_PTR data,
/* OUT */ RETURN_CODE * return_code)
{
  /* 1. Найти физический блок */
  FTL_INDEX m_pbi;
  RETURN_CODE m_get_error = NO_ERROR;
  FTL_BLOCK_GET(LBI, &m_pbi, &m_get_error);
  if(NO_ERROR != m_get_error)
  {
    STD_MEMSET(250U, 0xFF, data);
    *return_code = NO_ACTION;
    return;
  }

  /* 2. Прочитать блок из flash */
  U8 m_block[FTL_BLOCK_SIZE];
  FLASH_ADDRESS m_pba = m_pbi * FTL_BLOCK_SIZE + g_ftl_header.pba;
  RETURN_CODE m_read_error = NO_ERROR;
  FLASH_READ(m_pba, FTL_BLOCK_SIZE, m_block, &m_read_error);
  if(OPERATION_FAILED == m_read_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  /* 3. Извлечь метаданные и данные */
  FTL_BLOCK_TYPE m_meta;
  STD_MEMCPY(sizeof(FTL_BLOCK_TYPE), m_block, &m_meta);
  U8 m_data[FTL_BLOCK_SIZE - sizeof(FTL_BLOCK_TYPE)];
  STD_MEMCPY(
    FTL_BLOCK_SIZE - sizeof(FTL_BLOCK_TYPE),
    m_block + sizeof(FTL_BLOCK_TYPE), m_data
  );

  if(FTL_FLAG_VALID == m_meta.flag)
  {
    /* 4. Проверить CRC */
    U32 m_crc32 = 0U;
    HASH_CRC(m_data, FTL_BLOCK_SIZE - sizeof(FTL_BLOCK_TYPE), &m_crc32);

    if(m_crc32 != m_meta.crc32)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }

  /* 5. Расшифровать данные */
  //CRYPT_XOR(m_data, FTL_BLOCK_SIZE - sizeof(FTL_BLOCK_TYPE), m_pba);

  /* 6. Скопировать в выходной буфер */
  STD_MEMCPY(FTL_BLOCK_SIZE - sizeof(FTL_BLOCK_TYPE), m_data, data);

  *return_code = NO_ERROR;
}



void FTL_INIT(
/* OUT */ RETURN_CODE * return_code)
{
  if(FTL_MODE_SUPERVISOR != g_ftl_header.mode)
  {
    *return_code = ACCESS_DENIED;
    return;
  }

  RETURN_CODE m_flash_init = NO_ERROR;
  FLASH_INIT(&m_flash_init);
  if(NO_ERROR != m_flash_init)
  {
    *return_code = NO_ACTION;
    return;
  }

  /* 1. Восстановление таблицы из flash */
  FLASH_SECTOR_TYPE m_flash_sector;
  RETURN_CODE m_select_error = NO_ERROR;
  FLASH_SECTOR_SELECT(2U, &m_flash_sector, &m_select_error);
  if(NO_ERROR != m_select_error)
  {
    *return_code = NO_ACTION;
    return;
  }
  g_ftl_header.pba = m_flash_sector.pba;

  for(U32 i = 0U; i < FTL_BLOCKS_COUNT; i++)
  {
    FLASH_ADDRESS m_pba = i * FTL_BLOCK_SIZE + g_ftl_header.pba;
    FTL_BLOCK_TYPE m_meta;

    U8 m_buffer[8U];
    RETURN_CODE m_read_error = NO_ERROR;
    FLASH_READ(m_pba, 8U, m_buffer, &m_read_error);
    if(NO_ERROR != m_read_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
    STD_MEMCPY(sizeof(FTL_BLOCK_TYPE), m_buffer, &m_meta);

    g_ftl_header.table[i] = m_meta;
    if(FTL_FLAG_FREE == g_ftl_header.table[i].flag)
    {
      g_ftl_header.table[i] =
      (FTL_BLOCK_TYPE){
        .flag = FTL_FLAG_FREE,
        .lbi = 0U,
        .crc32 = 0U
      };
    }
  }

  g_ftl_header.mode = FTL_MODE_USER;

  *return_code = NO_ERROR;
}

void FTL_FREE(
/* OUT */ RETURN_CODE * return_code)
{
  g_ftl_header.mode = FTL_MODE_SUPERVISOR;
  /* TODO: garbage_collect */
  RETURN_CODE m_flash_free_error = NO_ERROR;
  FLASH_FREE(&m_flash_free_error);
  /* TODO: m_flash_free_error */
}

void FTL_WRITE(
/* IN  */ const FTL_INDEX LBI,
/* IN  */ const SIZE32 COUNT,
/* IN  */ const VOID_PTR DATA,
/* OUT */ RETURN_CODE * return_code)
{
  if((LBI + COUNT) >= FTL_BLOCKS_COUNT) /* TODO: check > or >= */
  {
    *return_code = INVALID_PARAM;
    return;
  }

  for(register FTL_INDEX i = 0U; i < COUNT; i++)
  {
    RETURN_CODE m_write_error = NO_ERROR;
    FTL_WRITE_BLOCK(LBI + i, (U8 *)DATA + i * FTL_BLOCK_SIZE, &m_write_error);
    if(NO_ERROR != m_write_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }
}

void FTL_READ(
/* IN  */ const FTL_INDEX LBI,
/* IN  */ const SIZE32 COUNT,
/* OUT */ VOID_PTR data,
/* OUT */ RETURN_CODE * return_code)
{
  if((LBI + COUNT) >= FTL_BLOCKS_COUNT) /* TODO: check > or >= */
  {
    *return_code = INVALID_PARAM;
    return;
  }

  for(register FTL_INDEX i = 0U; i < COUNT; i++)
  {
    RETURN_CODE m_read_error = NO_ERROR;
    FTL_READ_BLOCK(LBI + i, data + i * FTL_BLOCK_SIZE, &m_read_error);
    if(NO_ACTION == m_read_error)
    {
      *return_code = NO_ACTION;
      return;
    }
    if(OPERATION_FAILED == m_read_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }
}


void FTL_GARBAGE_COLLECT(
/* OUT */ RETURN_CODE * return_code)
{
  /* Проходим по каждому сектору */
  FLASH_SECTOR_ID m_sector_id = 2U;
  for(; m_sector_id < FLASH_SECTORS_COUNT; m_sector_id++)
  {
    FLASH_ADDRESS m_start_pba = 0x0;
    FLASH_ADDRESS m_end_pba = 0x0;

    /* no need to check m_select_error */
    RETURN_CODE m_borders_error = NO_ERROR;
    FLASH_SECTOR_BORDERS(
      m_sector_id, &m_start_pba, &m_end_pba, &m_borders_error
    );

    FTL_INDEX m_start_pbi = (m_start_pba - g_ftl_header.pba) / FTL_BLOCK_SIZE;
    FTL_INDEX m_end_pbi = (m_end_pba - g_ftl_header.pba) / FTL_BLOCK_SIZE;

    FTL_INDEX m_dirty_pbi = m_start_pbi;
    I32 m_sector_to_erase = 0;
    for(; m_dirty_pbi < m_end_pbi; m_dirty_pbi++)
    {
      if(g_ftl_header.table[m_dirty_pbi].flag == FTL_FLAG_DIRTY)
      {
        I32 m_sector_to_erase = 1;
        /* Найти свободный блок в следующих секторах */
        FTL_INDEX m_free_pbi = 0U;
        for(; m_free_pbi < FTL_BLOCKS_COUNT; m_free_pbi++)
        {
          if((m_free_pbi >= m_start_pbi) && (m_free_pbi < m_end_pbi))
          {
            continue;
          }
          if(g_ftl_header.table[m_free_pbi].flag == FTL_FLAG_FREE)
          {
            break;
          }
        }
        /* TODO: свободный блок не найден */

        /* Получить адреса старого и нового блока */
        U8 m_data[FTL_BLOCK_SIZE];
        const FLASH_ADDRESS M_DIRTY_PBA
          = m_dirty_pbi * FTL_BLOCK_SIZE + g_ftl_header.pba;
        const FLASH_ADDRESS M_FREE_PBA
          = m_free_pbi * FTL_BLOCK_SIZE + g_ftl_header.pba;

        /* Переместить блок */
        RETURN_CODE m_read_error = NO_ERROR;
        FLASH_READ(M_DIRTY_PBA, FTL_BLOCK_SIZE, m_data, &m_read_error);
        if(NO_ERROR != m_read_error)
        {
          *return_code = OPERATION_FAILED;
          return;
        }

        RETURN_CODE m_write_error = NO_ERROR;
        FLASH_WRITE(M_FREE_PBA, FTL_BLOCK_SIZE, m_data, &m_write_error);
        if(NO_ERROR != m_write_error)
        {
          *return_code = OPERATION_FAILED;
          return;
        }

        /* Обновить таблицу FTL */
        g_ftl_header.table[m_free_pbi] = g_ftl_header.table[m_dirty_pbi];
        g_ftl_header.table[m_dirty_pbi] =
        (FTL_BLOCK_TYPE){
          .flag = FTL_FLAG_FREE,
          .lbi = 0U,
          .crc32 = 0U
        };
      }
    }

    if(0 == m_sector_to_erase)
    {
      continue;
    }

    RETURN_CODE m_erase_error = NO_ERROR;
    FLASH_SECTOR_ERASE(m_sector_id, &m_erase_error);
    if(NO_ERROR != m_erase_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }
}
