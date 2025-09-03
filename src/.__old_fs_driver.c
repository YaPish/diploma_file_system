#include "fs_def.h"
#include "fs_std.h"
#include "fs_crypt.h"
#include "fs_ftl.h"
#include "fs_driver.h"

/*
 * Размер логического блока
 */
#define FS_BLOCK_SIZE 250U

/*
 * Размер данных в логическом блоке (если хранится адрес следующего блока)
 */
#define FS_DATA_SIZE 248U

/*
 * Количество блоков
 */
#define FS_BLOCKS_COUNT 3968U

/*
 * Количество файлов
 */
#define FS_FILES_COUNT 2000U

/*
 * Количество файловых дескрипторов (открытые файлы)
 */
#define FS_DESCRIPTORS_COUNT 128U

/*
 * Магическое число суперблока
 */
#define FS_MAGIC 0x46534653U

/*
 * Номер тега
 */
typedef U8 TAG_ID;

/*
 * Битовая карта блоков (2 бита на блок)
 */
typedef U8 BLOCK_FLAG_BITMAP[FS_BLOCKS_COUNT / 4U];

/*
 * ФЛАГИ БЛОКОВ:
 *   BLOCK_FLAG_SYSTEM: Системный блок (метаданные)
 *   BLOCK_FLAG_FREE: Блок свободен
 *   BLOCK_FLAG_USED: Блок занят
 */
typedef enum
{
  BLOCK_FLAG_SYSTEM = 0x01,
  BLOCK_FLAG_FREE   = 0x02,
  BLOCK_FLAG_USED   = 0x03
} BLOCK_FLAG;

/*
 * СУПЕРБЛОК:
 *   magic: Идентификатор блока
 *   (4 байт)
 */
typedef struct
{
  U32 magic;
} FS_SUPERBLOCK_TYPE;

/*
 * МЕТАДАННЫЕ ФАЙЛА:
 *   id: Уникальный номер
 *   lbi_start: LBI первого блока
 *   tags: Тэги
 *   size: Размер
 *   crc32: Контрольная сумма
 *   (23 байта)
 */
typedef struct
{
  FILE_ID     id;
  FTL_INDEX   lbi_start;
  TAG_BITMAP  tags;
  SIZE32      size;
  U32         crc32;
} FILE_HEADER_TYPE;

/*
 * СТРУКТУРА ДЕСКРИПТОРА ФАЙЛА:
 *   id: Системный номер
 *   status: Статус
 *   name: Имя
 *   header: Метаданные
 *   (95 байт)
 */
typedef struct
{
  FILE_ID id;
  FILE_STATUS_TYPE status;
  FILE_NAME name;
  FILE_HEADER_TYPE header;
} FS_DESCRIPTOR_TYPE;

/*
 * СУПЕРБЛОК (ОЗУ + FLASH, 30 байт (0,03 КБ))
 */
static FS_SUPERBLOCK_TYPE g_fs_superblock;

/*
 * БИТОВАЯ КАРТА БЛОКОВ (ОЗУ + FLASH, 1024 байт (1 КБ))
 */
static BLOCK_FLAG_BITMAP g_fs_block_flags;

/*
 * МАССИВ НАЗВАНИЙ ТЕГОВ (ОЗУ + FLASH, 19 байт * 52 = 988 байт (0,96 КБ))
 */
static TAG_NAME g_fs_tag_names[FS_TAGS_COUNT];

/*
 * ТАБЛИЦА ДЕСКРИПТОРОВ (ОЗУ, 95 байт * 128 = 12160 байт (11,875 КБ)):
 *   index: Дескриптор файла
 *   g_fs_descriptor_table[index]: Данные дескриптора
 */
static FS_DESCRIPTOR_TYPE g_fs_descriptor_table[FS_DESCRIPTORS_COUNT];

/*
 * ============ FLASH ============
 *
 * +-----------------------------+ BLOCK 0 (1 count)
 * |                             |
 * | SUPER_BLOCK                 |
 * |                             |
 * +-----------------------------+ BLOCK 1-5 (5 count)
 * |                             |
 * | BLOCK_FLAGS_BITMAP          |
 * |                             |
 * +-----------------------------+ BLOCK 6-9 (4 count)
 * |                             |
 * | TAG_NAMES (13 per block)    |
 * |                             |
 * +-----------------------------+ BLOCK 10-409 (400 count)
 * |                             |
 * | FILE_NAMES (5 per block)    |
 * |                             |
 * +-----------------------------+ BLOCK 410-609 (200 count)
 * |                             |
 * | FILE_HEADERS (10 per block) |
 * |                             |
 * +-----------------------------+ BLOCK 610+
 * |                             |
 * | DATA                        |
 * |                             |
 * +-----------------------------+
 */


/* ======== BLOCKNEXT ======== */
/*
 * ПОЛУЧИТЬ СЛЕДУЮЩИЙ БЛОК В ЦЕПОЧКЕ:
 *   LBI: Номер блока (в котором хранится значение следующего)
 *   next_lbi: Номер следующего блока
 *   return_code: Статус операции
 *     NO_ERROR: Номер успешно получен
 *     NO_ACTION: Блока не существует
 *     OPERATION_FAILED: Ошибка связи
 */
static void FS_BLOCKNEXT_READ(
/* IN  */ const FTL_INDEX LBI,
/* OUT */ FTL_INDEX * next_lbi,
/* OUT */ RETURN_CODE * return_code);

/*
 * УСТАНОВИТЬ СЛЕДУЮЩИЙ БЛОК В ЦЕПОЧКУ:
 *   LBI: Номер блока (в который устанавливается значение следующего)
 *   NEXT_LBI: Номер следующего блока
 *   return_code: Статус операции
 *     NO_ERROR: Номер успешно установлен
 *     NO_ACTION: Блока не существует
 *     OPERATION_FAILED: Ошибка связи
 */
static void FS_BLOCKNEXT_WRITE(
/* IN  */ const FTL_INDEX LBI,
/* IN  */ const FTL_INDEX NEXT_LBI,
/* OUT */ RETURN_CODE * return_code);
/* ======== BLOCKNEXT ======== */



/* ======== BLOCKFLAG ======== */
static void FS_BLOCKFLAG_READ(
/* IN  */ const FTL_INDEX LBI,
/* OUT */ BLOCK_FLAG * flag,
/* OUT */ RETURN_CODE * return_code);

static void FS_BLOCKFLAG_WRITE(
/* IN  */ const FTL_INDEX LBI,
/* IN  */ const BLOCK_FLAG FLAG,
/* OUT */ RETURN_CODE * return_code);
/* ======== BLOCKFLAG ======== */



/* ======== TAGNAME ======== */
/*
 * ЧТЕНИЕ ТЕГА:
 *   ID: Номер тега в системе
 *   name: Название тега
 *   return_code: Статус операции
 *     NO_ERROR: Название тега получено
 *     INVALID_PARAM: Номер тега не существует
 *     NO_ACTION: Тег пустой
 *     OPERATION_FAULT: Ошибка чтения
 */
static void FS_TAGNAME_READ(
/* IN  */ const TAG_ID ID,
/* OUT */ TAG_NAME name,
/* OUT */ RETURN_CODE * return_code);

/*
 * ЗАПИСЬ ТЕГА:
 *   ID: Номер тега в системе
 *   NAME: Название тега
 *   return_code: Статус операции
 *     NO_ERROR: Название тега записано
 *     INVALID_PARAM: Номер тега не существует
 *     OPERATION_FAULT: Ошибка записи
 */
static void FS_TAGNAME_WRITE(
/* IN  */ const TAG_ID ID,
/* IN  */ const TAG_NAME NAME,
/* OUT */ RETURN_CODE * return_code);
/* ======== TAGNAME ======== */



/* ======== FILENAME ======== */
/*
 * ЧТЕНИЕ ИМЕНИ ФАЙЛА:
 *   ID: Номер файла в системе
 *   name: Имя файла
 *   return_code: Статус операции
 *     NO_ERROR: Имя файла получено
 *     INVALID_PARAM: Номер выходит за границы
 *     NO_ACTION: Файла не существует
 *     OPERATION_FAULT: Ошибка чтения
 */
static void FS_FILENAME_READ(
/* IN  */ const FILE_ID ID,
/* OUT */ FILE_NAME name,
/* OUT */ RETURN_CODE * return_code);

/*
 * ЗАПИСЬ ИМЕНИ ФАЙЛА
 *   ID: Номер файла в системе
 *   NAME: Имя файла
 *   return_code: Статус операции
 *     NO_ERROR: Имя файла записано
 *     INVALID_PARAM: Номер выходит за границы
 *     OPERATION_FAULT: Ошибка записи
 */
static void FS_FILENAME_WRITE(
/* IN  */ const FILE_ID ID,
/* IN  */ const FILE_NAME NAME,
/* OUT */ RETURN_CODE * return_code);
/* ======== FILENAME ======== */



/* ======== FILEHEADER ======== */
/*
 * ЧТЕНИЕ ЗАГОЛОВКА ФАЙЛА:
 *   ID: Номер файла в системе
 *   header: Заголовок файла (метаданные)
 *   return_code: Статус операции
 *     NO_ERROR: Заголовок файла получен
 *     INVALID_PARAM: Номер выходит за границы
 *     NO_ACTION: Файла не существует
 *     OPERATION_FAULT: Ошибка чтения
 */
static void FS_FILEHEADER_READ(
/* IN  */ const FILE_ID ID,
/* OUT */ FILE_HEADER_TYPE * header,
/* OUT */ RETURN_CODE * return_code);

/*
 * ЗАПИСЬ ЗАГОЛОВКА ФАЙЛА
 *   ID: Номер файла в системе
 *   HEADER: Заголовок файла
 *   return_code: Статус операции
 *     NO_ERROR: Заголовок файла записан
 *     INVALID_PARAM: Номер выходит за границы
 *     OPERATION_FAULT: Ошибка записи
 */
static void FS_FILEHEADER_WRITE(
/* IN  */ const FILE_ID ID,
/* IN  */ const FILE_HEADER_TYPE HEADER,
/* OUT */ RETURN_CODE * return_code);
/* ======== FILEHEADER ======== */



/* ======== FILE ======== */
/*
 * ПОИСК ФАЙЛА ПО ИМЕНИ
 *   NAME: Имя файла
 *   id: Номер файла в системе
 *   return_code: Статус операции
 *     NO_ERROR: Файл найден
 *     NO_ACTION: Файла не существует
 *     OPERATION_FAULT: Ошибка поиска
 */
static void FS_FILE_FIND(
/* IN  */ const FILE_NAME NAME,
/* OUT */ FILE_ID * id,
/* OUT */ RETURN_CODE * return_code);
/* ======== FILE ======== */






/* ======== BLOCKNEXT ======== */
static void FS_BLOCKNEXT_READ(
/* IN  */ const FTL_INDEX LBI,
/* OUT */ FTL_INDEX * next_lbi,
/* OUT */ RETURN_CODE * return_code)
{
  U8 m_data[FS_BLOCK_SIZE];
  RETURN_CODE m_read_error = NO_ERROR;
  FTL_READ(LBI, 1U, m_data, &m_read_error);
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

  *next_lbi = (FTL_INDEX)((m_data[0] << 8U) | m_data[1]);

  *return_code = NO_ERROR;
}

static void FS_BLOCKNEXT_WRITE(
/* IN  */ const FTL_INDEX LBI,
/* IN  */ const FTL_INDEX NEXT_LBI,
/* OUT */ RETURN_CODE * return_code)
{
  U8 m_data[FS_BLOCK_SIZE];
  RETURN_CODE m_read_error = NO_ERROR;
  FTL_READ(LBI, 1U, m_data, &m_read_error);
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

  m_data[0U] = (U8)(NEXT_LBI >> 8U);
  m_data[1U] = (U8)(NEXT_LBI);
  RETURN_CODE m_write_error = NO_ERROR;
  FTL_WRITE(LBI, 1U, m_data, &m_write_error);
  if(NO_ERROR != m_write_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  *return_code = NO_ERROR;
}
/* ======== BLOCKNEXT ======== */



/* ======== BLOCKFLAG ======== */
static void FS_BLOCKFLAG_READ(
/* IN  */ const FTL_INDEX LBI,
/* OUT */ BLOCK_FLAG * flag,
/* OUT */ RETURN_CODE * return_code)
{
  /* TODO */
}

static void FS_BLOCKFLAG_WRITE(
/* IN  */ const FTL_INDEX LBI,
/* IN  */ const BLOCK_FLAG FLAG,
/* OUT */ RETURN_CODE * return_code)
{
  SIZE32 m_index = LBI / 4U;
  U8 m_shift = (LBI % 4U) * 2U;
  U8 m_mask = 0x03U << m_shift;

  g_fs_block_flags[m_index] &= ~m_mask;
  g_fs_block_flags[m_index] |= (FLAG << m_shift);

  RETURN_CODE m_bitmap_error = NO_ERROR;
  for(register U8 i = 0U; i < 4U; i++)
  {
    FTL_WRITE(1U + i, 1U, g_fs_block_flags + i * FS_BLOCK_SIZE, return_code);
    if(*return_code != NO_ERROR)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }

  *return_code = NO_ERROR;
}
/* ======== BLOCKFLAG ======== */



/* ======== TAGNAME ======== */
static void FS_TAGNAME_READ(
/* IN  */ const TAG_ID ID,
/* OUT */ TAG_NAME name,
/* OUT */ RETURN_CODE * return_code)
{

}

static void FS_TAGNAME_WRITE(
/* IN  */ const TAG_ID ID,
/* IN  */ const TAG_NAME NAME,
/* OUT */ RETURN_CODE * return_code)
{
  for(register SIZE32 i = 0U; i < 4U; i++)
  {
    RETURN_CODE m_write_error = NO_ERROR;
    FTL_WRITE(6U + i, 1U, g_fs_tag_names + i * FS_BLOCK_SIZE, &m_write_error);
    if(NO_ERROR != m_write_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }
}
/* ======== TAGNAME ======== */



/* ======== FILENAME ======== */
static void FS_FILENAME_READ(
/* IN  */ const FILE_ID ID,
/* OUT */ FILE_NAME name,
/* OUT */ RETURN_CODE * return_code)
{
  if(ID >= FS_FILES_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  SIZE32 m_block_id = 10U + ID / 5U;
  SIZE32 m_offset = (ID % 5U) * FILE_NAME_SIZE;

  U8 m_data[FS_BLOCK_SIZE];
  RETURN_CODE m_read_error = NO_ERROR;
  FTL_READ(m_block_id, 1U, m_data, &m_read_error);
  if(OPERATION_FAILED == m_read_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }
  if(NO_ACTION == m_read_error)
  {
    *return_code = NO_ACTION;
    return;
  }

  STD_MEMCPY(FILE_NAME_SIZE, m_data + m_offset, name);

  *return_code = NO_ERROR;
}

static void FS_FILENAME_WRITE(
/* IN  */ const FILE_ID ID,
/* IN  */ const FILE_NAME NAME,
/* OUT */ RETURN_CODE * return_code)
{
  if(ID >= FS_FILES_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  SIZE32 m_block_id = 10U + ID / 5U;
  SIZE32 m_offset = (ID % 5U) * FILE_NAME_SIZE;

  U8 m_data[FS_BLOCK_SIZE];
  RETURN_CODE m_read_error = NO_ERROR;
  FTL_READ(m_block_id, 1U, m_data, &m_read_error);
  if(OPERATION_FAILED == m_read_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  STD_MEMCPY(FILE_NAME_SIZE, (CHAR *)NAME, m_data + m_offset);
  RETURN_CODE m_write_error = NO_ERROR;
  FTL_WRITE(m_block_id, 1U, m_data, &m_write_error);
  if(NO_ERROR != m_write_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  *return_code = NO_ERROR;
}
/* ======== FILENAME ======== */



/* ======== FILEHEADER ======== */
static void FS_FILEHEADER_READ(
/* IN  */ const FILE_ID ID,
/* OUT */ FILE_HEADER_TYPE * header,
/* OUT */ RETURN_CODE * return_code)
{
  if(ID >= FS_FILES_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  SIZE32 m_block_id = 410U + ID / 10U;
  SIZE32 m_offset = (ID % 10U) * sizeof(FILE_HEADER_TYPE);

  U8 m_data[FS_BLOCK_SIZE];
  RETURN_CODE m_read_error = NO_ERROR;
  FTL_READ(m_block_id, 1U, m_data, &m_read_error);
  if(OPERATION_FAILED == m_read_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }
  if(NO_ACTION == m_read_error)
  {
    *return_code = NO_ACTION;
    return;
  }

  STD_MEMCPY(sizeof(FILE_HEADER_TYPE), m_data + m_offset, header);
}

static void FS_FILEHEADER_WRITE(
/* IN  */ const FILE_ID ID,
/* IN  */ const FILE_HEADER_TYPE HEADER,
/* OUT */ RETURN_CODE * return_code)
{
  if(ID >= FS_FILES_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  SIZE32 m_block_id = 410U + ID / 10U;
  SIZE32 m_offset = (ID % 10U) * sizeof(FILE_HEADER_TYPE);

  U8 m_data[FS_BLOCK_SIZE];
  RETURN_CODE m_read_error = NO_ERROR;
  FTL_READ(m_block_id, 1U, m_data, &m_read_error);
  if(OPERATION_FAILED == m_read_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  STD_MEMCPY(sizeof(FILE_HEADER_TYPE), (VOID_PTR)&HEADER, m_data + m_offset);
  RETURN_CODE m_write_error = NO_ERROR;
  FTL_WRITE(m_block_id, 1U, m_data, &m_write_error);
  if(NO_ERROR != m_write_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  *return_code = NO_ERROR;
}
/* ======== FILEHEADER ======== */



/* ======== FILE ======== */
static void FS_FILE_FIND(
/* IN  */ const FILE_NAME NAME,
/* OUT */ FILE_ID * id,
/* OUT */ RETURN_CODE * return_code)
{
  FILE_NAME m_current_name;

  for(register FILE_ID i = 0U; i < FS_FILES_COUNT; i++)
  {
    RETURN_CODE m_read_error = NO_ERROR;
    FS_FILENAME_READ(i, m_current_name, &m_read_error);
    if(OPERATION_FAILED == m_read_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
    if(NO_ACTION == m_read_error)
    {
      continue;
    }

    I32 m_cmp_result;
    STD_STRCMP(m_current_name, NAME, &m_cmp_result);
    if(m_cmp_result == 0L)
    {
      *id = i;
      *return_code = NO_ERROR;
      return;
    }
  }

  *id = (FILE_ID)UN_SET;
  *return_code = NO_ACTION;
}
/* ======== FILE ======== */





/*
 * ФОРМАТИРОВАНИЕ ФС
 */
static void FS_FORMAT(
/* OUT */ RETURN_CODE * return_code)
{
  g_fs_superblock.magic = FS_MAGIC;

  /* Инициализация битовой карты блоков */
  for(register FTL_INDEX m_lbi = 0U; m_lbi < 610U; m_lbi++)
  {
    RETURN_CODE m_blockflag_error = NO_ERROR;
    FS_BLOCKFLAG_WRITE(m_lbi, BLOCK_FLAG_SYSTEM, &m_blockflag_error);
    if(NO_ERROR != m_blockflag_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }
  for(register FTL_INDEX m_lbi = 610U; m_lbi < FS_BLOCKS_COUNT; m_lbi++)
  {
    RETURN_CODE m_blockflag_error = NO_ERROR;
    FS_BLOCKFLAG_WRITE(m_lbi, BLOCK_FLAG_FREE, &m_blockflag_error);
    if(NO_ERROR != m_blockflag_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }

  /* Инициализация имен тегов */
  TAG_NAME m_tag_empty = {0};
  for(register TAG_ID m_tag_id = 0U; m_tag_id < FS_TAGS_COUNT; m_tag_id++)
  {
    RETURN_CODE m_tag_error = NO_ERROR;
    FS_TAGNAME_WRITE(m_tag_id, m_tag_empty, &m_tag_error);
    if(NO_ERROR != m_tag_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }

  /* Очистка имен файлов */
  FILE_NAME m_empty_name = {0};
  for(register FILE_ID i = 0U; i < FS_FILES_COUNT; i++)
  {
    RETURN_CODE m_filename_error = NO_ERROR;
    FS_FILENAME_WRITE(i, m_empty_name, &m_filename_error);
    if(NO_ERROR != m_filename_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }
  RETURN_CODE m_gb_error = NO_ERROR;
  FTL_GARBAGE_COLLECT(&m_gb_error);
  /* TODO */

  /* Очистка заголовков файлов */
  FILE_HEADER_TYPE m_empty_header = {0};
  for(register FILE_ID i = 0U; i < FS_FILES_COUNT; i++)
  {
    RETURN_CODE m_write_error = NO_ERROR;
    FS_FILEHEADER_WRITE(i, m_empty_header, &m_write_error);
    if(NO_ERROR != m_write_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }
  FTL_GARBAGE_COLLECT(&m_gb_error);
  /* TODO */

  *return_code = NO_ERROR;
}



/*
 * ИНИЦИАЛИЗАЦИЯ ФС
 */
void FS_INIT(RETURN_CODE* return_code)
{
  RETURN_CODE m_ftl_init_error = NO_ERROR;
  FTL_INIT(&m_ftl_init_error);
  if(NO_ERROR != m_ftl_init_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  /* Чтение суперблока */
  RETURN_CODE m_superblock_error = NO_ERROR;
  FTL_READ(0U, 1U, &g_fs_superblock, &m_superblock_error);
  if(OPERATION_FAILED == m_superblock_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }
  if((NO_ACTION == m_superblock_error)
  || (g_fs_superblock.magic != FS_MAGIC))
  {
    RETURN_CODE m_format_error = NO_ERROR;
    FS_FORMAT(&m_format_error);
    /* TODO */
    return;
  }

  /* Чтение битовой карты блоков */
  for(register FTL_INDEX m_lbi = 0U; m_lbi < FS_BLOCKS_COUNT; m_lbi++)
  {
    BLOCK_FLAG m_tmp_flag;
    RETURN_CODE m_blockflag_error = NO_ERROR;
    FS_BLOCKFLAG_READ(m_lbi, &m_tmp_flag, &m_blockflag_error);
    if(NO_ERROR != m_blockflag_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }

  /* Чтение имен тегов */
  for(register TAG_ID m_tag_id = 0U; m_tag_id < FS_TAGS_COUNT; m_tag_id++)
  {
    RETURN_CODE m_tag_error = NO_ERROR;
    FS_TAGNAME_READ(m_tag_id, g_fs_tag_names[m_tag_id], &m_tag_error);
    if(OPERATION_FAILED == m_tag_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }

  /* Инициализация таблицы дескрипторов */
  for(register SIZE32 i = 0U; i < FS_DESCRIPTORS_COUNT; i++)
  {
    g_fs_descriptor_table[i].id = (FILE_ID)UN_SET;
  }

  *return_code = NO_ERROR;
}



void FS_FREE(RETURN_CODE * return_code)
{
  /* TODO: запись в память (если что-то осталось) */
  FTL_FREE(return_code);
}







/*
 * ВЫДЕЛЕНИЕ БЛОКА:
 *   lbi: Индекс логического блока
 *   return_code: Состояние операции
 */
void FS_BLOCK_ALLOCATE(
/* OUT */ FTL_INDEX * lbi,
/* OUT */ RETURN_CODE * return_code)
{
  SIZE32 m_byte_idx;
  U8 m_byte;
  U8 m_shift;
  BLOCK_FLAG m_flag;

  for(FTL_INDEX m_index = 610U; m_index < FS_BLOCKS_COUNT; m_index++)
  {
    m_byte_idx = m_index / 4U;
    m_shift = (m_index % 4U) * 2U;
    m_byte = g_fs_block_flags[m_byte_idx];
    m_flag = (BLOCK_FLAG)((m_byte >> m_shift) & 0x03U);

    if(m_flag == BLOCK_FLAG_FREE)
    {
      *lbi = m_index;
      RETURN_CODE m_write_error = NO_ERROR;
      FS_BLOCKFLAG_WRITE(m_index, BLOCK_FLAG_USED, &m_write_error);
      if(NO_ERROR != m_write_error)
      {
        *return_code = OPERATION_FAILED;
      }
      else
      {
        *return_code = NO_ERROR;
      }
      return;
    }
  }

  *return_code = NO_ACTION;
}






void FS_FILE_CREATE(
/* IN  */ const FILE_NAME NAME,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error)
{
  /* Поиск свободного слота */
  //FILE_NAME m_empty_name;
  FILE_ID m_file_id;
  FILE_NAME m_current_name;

  for(m_file_id = 0U; m_file_id < FS_FILES_COUNT; m_file_id++)
  {
    RETURN_CODE m_read_error = NO_ERROR;
    FS_FILENAME_READ(m_file_id, m_current_name, &m_read_error);
    if(OPERATION_FAILED == m_read_error)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
    if(NO_ACTION == m_read_error)
    {
      continue;
    }

    I32 m_cmp_result;
    STD_STRCMP(m_current_name, NAME, &m_cmp_result);
    if(m_cmp_result == 0L)
    {
      break;
    }
  }

  if(m_file_id >= FS_FILES_COUNT)
  {
    *return_code = OPERATION_FAILED;
    *file_error = FILE_ERROR_NO_SPACE;
    return;
  }

  /* Выделение блока */
  FTL_INDEX m_lbi;
  FS_BLOCK_ALLOCATE(&m_lbi, return_code);
  if(*return_code != NO_ERROR)
  {
    *return_code = OPERATION_FAILED;
    *file_error = FILE_ERROR_NO_SPACE;
    return;
  }

  /* Инициализация заголовка */
  FILE_HEADER_TYPE m_header;
  m_header.id = m_file_id;
  m_header.lbi_start = m_lbi;
  m_header.size = 0U;
  m_header.crc32 = 0U;
  STD_MEMSET(sizeof(TAG_BITMAP), 0U, m_header.tags);

  /* Запись начального блока */
  U8 m_block_data[FS_BLOCK_SIZE];
  m_block_data[0] = (U8)(UN_SET >> 8U);
  m_block_data[1] = (U8)UN_SET;
  STD_MEMSET(FS_DATA_SIZE, 0U, m_block_data + 2U);

  FTL_WRITE(m_lbi, 1U, m_block_data, return_code);
  if(*return_code != NO_ERROR)
  {
    *file_error = FILE_ERROR_IO;
    return;
  }

  /* Сохранение метаданных */
  RETURN_CODE m_filename_error = NO_ERROR;
  FS_FILENAME_WRITE(m_file_id, NAME, &m_filename_error);
  if(NO_ERROR != m_filename_error)
  {
    *file_error = FILE_ERROR_IO;
    *return_code = OPERATION_FAILED;
    return;
  }

  RETURN_CODE m_fileheader_error = NO_ERROR;
  FS_FILEHEADER_WRITE(m_file_id, m_header, &m_fileheader_error);
  if(NO_ERROR != m_fileheader_error)
  {
    *file_error = FILE_ERROR_IO;
    *return_code = OPERATION_FAILED;
    return;
  }

  *return_code = NO_ERROR;
}

void FS_FILE_OPEN(
/* IN  */ const FILE_NAME NAME,
/* IN  */ const FILE_MODE MODE,
/* OUT */ FILE_ID * id,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error)
{
  /* Поиск файла */
  FILE_ID m_sys_id;
  RETURN_CODE m_find_error = NO_ERROR;
  FS_FILE_FIND(NAME, &m_sys_id, &m_find_error);
  if(NO_ERROR != m_find_error)
  {
    *file_error = FILE_ERROR_IO;
    *return_code = NO_ACTION;
    return;
  }

  if(m_sys_id == (FILE_ID)UN_SET)
  {
    *return_code = OPERATION_FAILED;
    *file_error = FILE_ERROR_NO_FILE;
    return;
  }

  /* Проверка на дублирование открытия */
  for(register SIZE32 i = 0U; i < FS_DESCRIPTORS_COUNT; i++)
  {
    if(g_fs_descriptor_table[i].id == m_sys_id)
    {
      *return_code = OPERATION_FAILED;
      *file_error = FILE_ERROR_BUSY;
      return;
    }
  }

  /* Поиск свободного дескриптора */
  register SIZE32 m_desc_id;
  for(m_desc_id = 0U; m_desc_id < FS_DESCRIPTORS_COUNT; m_desc_id++)
  {
    if(g_fs_descriptor_table[m_desc_id].id == (FILE_ID)UN_SET)
    {
      break;
    }
  }

  if(m_desc_id >= FS_DESCRIPTORS_COUNT)
  {
    *return_code = OPERATION_FAILED;
    *file_error = FILE_ERROR_BUSY;
    return;
  }

  /* Загрузка метаданных */
  RETURN_CODE m_read_error = NO_ERROR;
  FS_FILEHEADER_READ(
    m_sys_id, &g_fs_descriptor_table[m_desc_id].header, &m_read_error
  );
  if(NO_ERROR != m_read_error)
  {
    *file_error = FILE_ERROR_IO;
    *return_code = OPERATION_FAILED;
    return;
  }

  g_fs_descriptor_table[m_desc_id].id = m_sys_id;
  g_fs_descriptor_table[m_desc_id].status.size =
    g_fs_descriptor_table[m_desc_id].header.size;
  g_fs_descriptor_table[m_desc_id].status.position = 0U;
  g_fs_descriptor_table[m_desc_id].status.mode = MODE;
  STD_MEMCPY(
    sizeof(TAG_BITMAP),
    g_fs_descriptor_table[m_desc_id].header.tags,
    g_fs_descriptor_table[m_desc_id].status.tags);

  STD_STRNCPY(FILE_NAME_SIZE, NAME, g_fs_descriptor_table[m_desc_id].name);

  *id = (FILE_ID)m_desc_id;
  *return_code = NO_ERROR;
}

void FS_FILE_CLOSE(
/* IN  */ const FILE_ID ID,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error)
{
  if(ID >= FS_DESCRIPTORS_COUNT)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_DESCRIPTOR;
    return;
  }

  if(g_fs_descriptor_table[ID].id == (FILE_ID)UN_SET)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_DESCRIPTOR;
    return;
  }

  /* Обновление размера файла при необходимости */
  if(g_fs_descriptor_table[ID].status.position
  != g_fs_descriptor_table[ID].header.size)
  {
    RETURN_CODE m_update_error = NO_ERROR;
    FS_FILESIZE_UPDATE(
      g_fs_descriptor_table[ID].id,
      g_fs_descriptor_table[ID].status.size,
      &m_update_error
    );
    if(NO_ERROR != m_update_error)
    {
      *file_error = FILE_ERROR_IO;
      *return_code = OPERATION_FAILED;
      return;
    }
  }

  g_fs_descriptor_table[ID].id = (FILE_ID)UN_SET;
  *return_code = NO_ERROR;
}

void FS_FILE_READ(
/* IN  */ const FILE_ID ID,
/* IN  */ const SIZE32 IN_LENGTH,
/* OUT */ SIZE32 * out_length,
/* OUT */ VOID_PTR data,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error)
{
  if(ID >= FS_DESCRIPTORS_COUNT)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_DESCRIPTOR;
    return;
  }

  if(g_fs_descriptor_table[ID].id == (FILE_ID)UN_SET)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_DESCRIPTOR;
    return;
  }

  FS_DESCRIPTOR_TYPE* m_desc = &g_fs_descriptor_table[ID];

  if(m_desc->status.position > m_desc->header.size)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_OVERFLOW;
    return;
  }

  SIZE32 m_bytes_left = m_desc->header.size - m_desc->status.position;
  SIZE32 m_to_read = IN_LENGTH;
  if(m_to_read > m_bytes_left)
  {
    m_to_read = m_bytes_left;
  }
  *out_length = m_to_read;

  if(m_to_read == 0U)
  {
    *return_code = NO_ERROR;
    return;
  }

  U8* m_data_ptr = (U8*)data;
  SIZE32 m_remaining = m_to_read;
  SIZE32 m_current_pos = m_desc->status.position;
  FTL_INDEX m_current_block = m_desc->header.lbi_start;

  /* Пропуск блоков до текущей позиции */
  SIZE32 m_blocks_to_skip = m_current_pos / FS_DATA_SIZE;
  for(SIZE32 i = 0U; i < m_blocks_to_skip; i++)
  {
    BLOCK_NEXT_GET(m_current_block, &m_current_block, return_code);
    if(*return_code != NO_ERROR)
    {
      *file_error = FILE_ERROR_IO;
      return;
    }
  }

  SIZE32 m_block_offset = m_current_pos % FS_DATA_SIZE;

  while(m_remaining > 0U)
  {
    U8 m_block_data[FS_BLOCK_SIZE];
    FTL_READ(m_current_block, 1U, m_block_data, return_code);
    if(*return_code != NO_ERROR)
    {
      *file_error = FILE_ERROR_IO;
      return;
    }

    SIZE32 m_available = FS_DATA_SIZE - m_block_offset;
    SIZE32 m_chunk_size = m_remaining;
    if(m_remaining > m_available)
    {
      m_chunk_size = m_available;
    }

    STD_MEMCPY(m_chunk_size, m_block_data + 2U + m_block_offset, m_data_ptr);

    m_data_ptr += m_chunk_size;
    m_remaining -= m_chunk_size;
    m_block_offset = 0U;

    if(m_remaining > 0U)
    {
      BLOCK_NEXT_GET(m_current_block, &m_current_block, return_code);
      if(*return_code != NO_ERROR)
      {
        *file_error = FILE_ERROR_IO;
        return;
      }
    }
  }

  m_desc->status.position += m_to_read;
  *return_code = NO_ERROR;
}

void FS_FILE_WRITE(
/* IN  */ const FILE_ID ID,
/* IN  */ const SIZE32 LENGTH,
/* IN  */ const VOID_PTR DATA,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error)
{
  if(ID >= FS_DESCRIPTORS_COUNT)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_DESCRIPTOR;
    return;
  }

  if(g_fs_descriptor_table[ID].id == (FILE_ID)UN_SET)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_DESCRIPTOR;
    return;
  }

  FS_DESCRIPTOR_TYPE* m_desc = &g_fs_descriptor_table[ID];

  if(m_desc->status.mode == FILE_MODE_READ_ONLY)
  {
    *return_code = ACCESS_DENIED;
    *file_error = FILE_ERROR_PERMISSION;
    return;
  }

  SIZE32 m_end_pos = m_desc->status.position + LENGTH;
  if(m_end_pos > FS_MAX_FILE_SIZE)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_FILE_SIZE;
    return;
  }

  if(m_end_pos > m_desc->header.size)
  {
    m_desc->header.size = m_end_pos;
  }

  const U8* m_data_ptr = (const U8*)DATA;
  SIZE32 m_remaining = LENGTH;
  SIZE32 m_current_pos = m_desc->status.position;
  FTL_INDEX m_current_block = m_desc->header.lbi_start;

  /* Пропуск блоков до текущей позиции */
  SIZE32 m_blocks_to_skip = m_current_pos / FS_DATA_SIZE;
  for(SIZE32 i = 0U; i < m_blocks_to_skip; i++)
  {
    BLOCK_NEXT_GET(m_current_block, &m_current_block, return_code);
    if(*return_code != NO_ERROR)
    {
      *file_error = FILE_ERROR_IO;
      return;
    }
  }

  SIZE32 m_block_offset = m_current_pos % FS_DATA_SIZE;

  while(m_remaining > 0U)
  {
    U8 m_block_data[FS_BLOCK_SIZE];
    FTL_READ(m_current_block, 1U, m_block_data, return_code);
    if(*return_code != NO_ERROR)
    {
      *file_error = FILE_ERROR_IO;
      return;
    }

    SIZE32 m_available = FS_DATA_SIZE - m_block_offset;
    SIZE32 m_chunk_size = m_remaining;
    if(m_remaining > m_available)
    {
      m_chunk_size = m_available;
    }

    STD_MEMCPY(m_chunk_size, (VOID_PTR)m_data_ptr,
      m_block_data + 2U + m_block_offset);
    FTL_WRITE(m_current_block, 1U, m_block_data, return_code);
    if(*return_code != NO_ERROR)
    {
      *file_error = FILE_ERROR_IO;
      return;
    }

    m_data_ptr += m_chunk_size;
    m_remaining -= m_chunk_size;
    m_block_offset = 0U;

    if(m_remaining > 0U)
    {
      FTL_INDEX m_next_block;
      BLOCK_NEXT_GET(m_current_block, &m_next_block, return_code);
      if(*return_code != NO_ERROR)
      {
        *file_error = FILE_ERROR_IO;
        return;
      }

      if(m_next_block == UN_SET)
      {
        FTL_INDEX m_new_block;
        FS_BLOCK_ALLOCATE(&m_new_block, return_code);
        if(*return_code != NO_ERROR)
        {
          *file_error = FILE_ERROR_NO_SPACE;
          return;
        }

        /* Инициализация нового блока */
        U8 m_new_block_data[FS_BLOCK_SIZE];
        m_new_block_data[0] = (U8)(UN_SET >> 8U);
        m_new_block_data[1] = (U8)UN_SET;
        STD_MEMSET(FS_DATA_SIZE, 0U, m_new_block_data + 2U);
        FTL_WRITE(m_new_block, 1U, m_new_block_data, return_code);
        if(*return_code != NO_ERROR)
        {
          *file_error = FILE_ERROR_IO;
          return;
        }

        /* Обновление цепочки блоков */
        BLOCK_NEXT_SET(m_current_block, m_new_block, return_code);
        if(*return_code != NO_ERROR)
        {
          *file_error = FILE_ERROR_IO;
          return;
        }
        m_current_block = m_new_block;
      }
      else
      {
        m_current_block = m_next_block;
      }
    }
  }

  m_desc->status.position += LENGTH;
  *return_code = NO_ERROR;
}

void FS_FILE_SEEK(
/* IN  */ const FILE_ID ID,
/* IN  */ const FILE_POSITION OFFSET,
/* IN  */ const FILE_SEEK WHENCE,
/* OUT */ FILE_POSITION * position,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error)
{
  if(ID >= FS_DESCRIPTORS_COUNT)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_DESCRIPTOR;
    return;
  }

  if(g_fs_descriptor_table[ID].id == (FILE_ID)UN_SET)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_DESCRIPTOR;
    return;
  }

  FS_DESCRIPTOR_TYPE* m_desc = &g_fs_descriptor_table[ID];
  I32 m_new_position;

  switch(WHENCE)
  {
    case FILE_SEEK_SET:
      m_new_position = OFFSET;
      break;

    case FILE_SEEK_CUR:
      m_new_position = m_desc->status.position + OFFSET;
      break;

    case FILE_SEEK_END:
      m_new_position = m_desc->header.size + OFFSET;
      break;

    default:
      *return_code = INVALID_PARAM;
      *file_error = FILE_ERROR_INVALID_PARAM;
      return;
  }

  if(m_new_position < 0 || (SIZE32)m_new_position > m_desc->header.size)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_OVERFLOW;
    return;
  }

  m_desc->status.position = (SIZE32)m_new_position;
  *position = m_desc->status.position;
  *return_code = NO_ERROR;
}

void FS_FILE_REMOVE(
/* IN  */ const FILE_NAME NAME,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error)
{
  FILE_ID m_sys_id;
  RETURN_CODE m_find_error = NO_ERROR;
  FS_FILE_FIND(NAME, &m_sys_id, &m_find_error);
  if(NO_ERROR != m_find_error)
  {
    *file_error = FILE_ERROR_IO;
    *return_code = OPERATION_FAILED;
    return;
  }

  if(m_sys_id == (FILE_ID)UN_SET)
  {
    *return_code = OPERATION_FAILED;
    *file_error = FILE_ERROR_NO_FILE;
    return;
  }

  /* Проверка на открытый файл */
  for(register SIZE32 i = 0U; i < FS_DESCRIPTORS_COUNT; i++)
  {
    if(g_fs_descriptor_table[i].id == m_sys_id)
    {
      *return_code = OPERATION_FAILED;
      *file_error = FILE_ERROR_BUSY;
      return;
    }
  }

  /* Освобождение блоков */
  FILE_HEADER_TYPE m_header;
  RETURN_CODE m_read_error = NO_ERROR;
  FS_FILEHEADER_READ(m_sys_id, &m_header, &m_read_error);
  if(NO_ERROR != m_read_error)
  {
    *file_error = FILE_ERROR_IO;
    *return_code = OPERATION_FAILED;
    return;
  }

  FTL_INDEX m_current_block = m_header.lbi_start;
  while(m_current_block != UN_SET)
  {
    FTL_INDEX m_next_block;
    RETURN_CODE m_get_error = NO_ERROR;
    BLOCK_NEXT_GET(m_current_block, &m_next_block, &m_get_error);
    if(OPERATION_FAILED == m_get_error)
    {
      *file_error = FILE_ERROR_IO;
      *return_code = OPERATION_FAILED;
      return;
    }
    if(NO_ACTION == m_get_error)
    {
      m_next_block = UN_SET;
    }

    RETURN_CODE m_bitmap_error = NO_ERROR;
    FS_BITMAP_SET(m_current_block, BLOCK_FLAG_FREE, &m_bitmap_error);
    if(NO_ERROR != m_bitmap_error)
    {
      *file_error = FILE_ERROR_IO;
      *return_code = OPERATION_FAILED;
      return;
    }

    m_current_block = m_next_block;
  }

  /* Очистка метаданных */
  FILE_NAME m_empty_name;
  m_empty_name[0] = '\0';
  RETURN_CODE m_filename_error = NO_ERROR;
  FS_FILENAME_WRITE(m_sys_id, m_empty_name, &m_filename_error);
  if(NO_ERROR != m_filename_error)
  {
    *file_error = FILE_ERROR_IO;
    *return_code = OPERATION_FAILED;
    return;
  }

  FILE_HEADER_TYPE m_empty_header = {0};
  RETURN_CODE m_fileheader_error = NO_ERROR;
  FS_FILEHEADER_WRITE(m_sys_id, m_empty_header, &m_fileheader_error);
  if(NO_ERROR != m_fileheader_error)
  {
    *file_error = FILE_ERROR_IO;
    *return_code = OPERATION_FAILED;
    return;
  }

  *return_code = NO_ERROR;
}

void FS_FILE_RENAME(
/* IN  */ const FILE_NAME OLD_NAME,
/* IN  */ const FILE_NAME NEW_NAME,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error)
{
  FILE_ID m_sys_id;
  RETURN_CODE m_find_old_error = NO_ERROR;
  FS_FILE_FIND(OLD_NAME, &m_sys_id, &m_find_old_error);
  if(NO_ERROR != m_find_old_error)
  {
    *file_error = FILE_ERROR_IO;
    *return_code = OPERATION_FAILED;
    return;
  }

  if(m_sys_id == (FILE_ID)UN_SET)
  {
    *return_code = OPERATION_FAILED;
    *file_error = FILE_ERROR_NO_FILE;
    return;
  }

  /* Проверка на существование нового имени */
  FILE_ID m_temp_id;
  RETURN_CODE m_find_new_error = NO_ERROR;
  FS_FILE_FIND(NEW_NAME, &m_temp_id, &m_find_new_error);
  if(NO_ACTION != m_find_new_error)
  {
    *file_error = FILE_ERROR_IO;
    *return_code = OPERATION_FAILED;
    return;
  }

  if(m_temp_id != (FILE_ID)UN_SET)
  {
    *return_code = OPERATION_FAILED;
    *file_error = FILE_ERROR_EXIST;
    return;
  }

  /* Обновление имени */
  RETURN_CODE m_write_error = NO_ERROR;
  FS_FILENAME_WRITE(m_sys_id, NEW_NAME, &m_write_error);
  if(NO_ERROR != m_write_error)
  {
    *file_error = FILE_ERROR_IO;
    *return_code = OPERATION_FAILED;
    return;
  }

  *return_code = NO_ERROR;
}

void FS_FILE_STATUS(
/* IN  */ const FILE_ID ID,
/* OUT */ FILE_STATUS_TYPE * status,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error)
{
  if(ID >= FS_DESCRIPTORS_COUNT)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_DESCRIPTOR;
    return;
  }

  if(g_fs_descriptor_table[ID].id == (FILE_ID)UN_SET)
  {
    *return_code = INVALID_PARAM;
    *file_error = FILE_ERROR_DESCRIPTOR;
    return;
  }

  *status = g_fs_descriptor_table[ID].status;
  *return_code = NO_ERROR;
}

void FS_TAG_ADD(
/* IN  */ const FILE_NAME NAME,
/* IN  */ const TAG_NAME TAG,
/* IN  */ RETURN_CODE * return_code)
{
  FILE_ID m_sys_id;
  RETURN_CODE m_find_error = NO_ERROR;
  FS_FILE_FIND(NAME, &m_sys_id, &m_find_error);
  if(NO_ERROR != m_find_error)
  {
    *return_code = NO_ACTION;
    return;
  }

  if(m_sys_id == (FILE_ID)UN_SET)
  {
    *return_code = NO_ACTION;
    return;
  }

  /* Поиск тега */
  register SIZE32 m_tag_idx;
  I32 m_cmp_result;
  for(m_tag_idx = 0U; m_tag_idx < FS_TAGS_COUNT; m_tag_idx++)
  {
    STD_STRCMP(g_fs_tag_names[m_tag_idx], TAG, &m_cmp_result);
    if(m_cmp_result == 0)
    {
      break;
    }
  }

  if(m_tag_idx >= FS_TAGS_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  /* Обновление битовой карты */
  FILE_HEADER_TYPE m_header;
  RETURN_CODE m_read_error = NO_ERROR;
  FS_FILEHEADER_READ(m_sys_id, &m_header, &m_read_error);
  if(NO_ERROR != m_read_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  U8 m_byte_idx = (U8)(m_tag_idx / 8U);
  U8 m_bit_idx = (U8)(m_tag_idx % 8U);
  m_header.tags[m_byte_idx] |= (1U << m_bit_idx);

  RETURN_CODE m_write_error = NO_ERROR;
  FS_FILEHEADER_WRITE(m_sys_id, m_header, &m_write_error);
  if(NO_ERROR != m_write_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  *return_code = NO_ERROR;
}

void FS_TAG_REMOVE(
/* IN  */ const FILE_NAME NAME,
/* IN  */ const TAG_NAME TAG,
/* OUT */ RETURN_CODE * return_code)
{
  FILE_ID m_sys_id;
  RETURN_CODE m_find_error = NO_ERROR;
  FS_FILE_FIND(NAME, &m_sys_id, &m_find_error);
  if(NO_ERROR != m_find_error)
  {
    *return_code = NO_ACTION;
    return;
  }

  if(m_sys_id == (FILE_ID)UN_SET)
  {
    *return_code = NO_ACTION;
    return;
  }

  /* Поиск тега */
  register SIZE32 m_tag_idx;
  I32 m_cmp_result;
  for(m_tag_idx = 0U; m_tag_idx < FS_TAGS_COUNT; m_tag_idx++)
  {
    STD_STRCMP(g_fs_tag_names[m_tag_idx], TAG, &m_cmp_result);
    if(m_cmp_result == 0)
    {
      break;
    }
  }

  if(m_tag_idx >= FS_TAGS_COUNT)
  {
    *return_code = INVALID_PARAM;
    return;
  }

  /* Обновление битовой карты */
  FILE_HEADER_TYPE m_header;
  RETURN_CODE m_read_error = NO_ERROR;
  FS_FILEHEADER_READ(m_sys_id, &m_header, &m_read_error);
  if(NO_ERROR != m_read_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  U8 m_byte_idx = (U8)(m_tag_idx / 8U);
  U8 m_bit_idx = (U8)(m_tag_idx % 8U);
  m_header.tags[m_byte_idx] &= ~(1U << m_bit_idx);

  RETURN_CODE m_write_error = NO_ERROR;
  FS_FILEHEADER_WRITE(m_sys_id, m_header, &m_write_error);
  if(NO_ERROR != m_write_error)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  *return_code = NO_ERROR;
}

void FS_TAG_RENAME(
/* IN  */ const TAG_NAME OLD_NAME,
/* IN  */ const TAG_NAME NEW_NAME,
/* OUT */ RETURN_CODE * return_code)
{
  SIZE32 m_tag_idx;
  I32 m_cmp_result;
  for(m_tag_idx = 0U; m_tag_idx < FS_TAGS_COUNT; m_tag_idx++)
  {
    STD_STRCMP(g_fs_tag_names[m_tag_idx], OLD_NAME, &m_cmp_result);
    if(m_cmp_result == 0)
    {
      break;
    }
  }

  if(m_tag_idx >= FS_TAGS_COUNT)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  /* Проверка на дублирование */
  for(SIZE32 i = 0U; i < FS_TAGS_COUNT; i++)
  {
    if(i == m_tag_idx)
    {
      continue;
    }

    STD_STRCMP(g_fs_tag_names[i], NEW_NAME, &m_cmp_result);
    if(m_cmp_result == 0)
    {
      *return_code = OPERATION_FAILED;
      return;
    }
  }

  /* Обновление имени тега */
  STD_STRNCPY(TAG_NAME_SIZE, NEW_NAME, g_fs_tag_names[m_tag_idx]);

  /* Сохранение на flash */
  SIZE32 m_block = 6U + m_tag_idx / (FS_BLOCK_SIZE / TAG_NAME_SIZE);
  SIZE32 m_offset =
    (m_tag_idx % (FS_BLOCK_SIZE / TAG_NAME_SIZE)) * TAG_NAME_SIZE;

  U8 m_block_data[FS_BLOCK_SIZE];
  FTL_READ(m_block, 1U, m_block_data, return_code);
  if(*return_code != NO_ERROR)
  {
    return;
  }

  STD_MEMCPY(TAG_NAME_SIZE, g_fs_tag_names[m_tag_idx], m_block_data + m_offset);
  FTL_WRITE(m_block, 1U, m_block_data, return_code);
}
