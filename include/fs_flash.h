#ifndef __FS_FLASH_H__
#define __FS_FLASH_H__

#include "fs_def.h"

/*
 * Количество флеш секторов
 */
#define FLASH_SECTORS_COUNT 12U

/*
 * Адрес flash-памяти
 */
typedef U32 FLASH_ADDRESS;

/*
 * Слово (4 байта)
 */
typedef U32 FLASH_WORD;

/*
 * Номер flash-сектора
 */
typedef U8 FLASH_SECTOR_ID;

/* УРОВЕНЬ ДОСТУПА:
 *   FLASH_ACCESS_SUPERVISOR: Только для служебных данных (чтение/запись)
 *   FLASH_ACCESS_READ_ONLY: Запись запрещена (только чтение)
 *   FLASH_ACCESS_READ_WRITE: Полный доступ (пользовательские данные)
 */
typedef enum
{
  FLASH_ACCESS_SUPERVISOR,
  FLASH_ACCESS_READ_ONLY,
  FLASH_ACCESS_READ_WRITE
} FLASH_ACCESS;

/*
 * РЕЖИМ РАБОТЫ:
 *   FLASH_MODE_SUPERVISOR: Привелигерованный режим
 *   FLASH_MODE_USER: Ограниченный доступ (пользовательский режим)
 */
typedef enum {
  FLASH_MODE_SUPERVISOR,
  FLASH_MODE_USER
} FLASH_MODE;


/* СЕКТОР:
 *   pba: Адреса начала сектора
 *   wear: Счетчик стираний сектора
 *   permission: Права сектора
 *   crc32: CRC32 хеш
 *   (12 + enum)
 */
typedef struct {
  FLASH_ADDRESS pba;
  FLASH_ACCESS permission;
  SIZE32 wear;
  U32 crc32;
} FLASH_SECTOR_TYPE;



/*
 * ИНИЦИАЛИЗАЦИЯ FLASH-ДРАЙВЕРА:
 *   return_code: Статус операции
 *     NO_ERROR: Драйвер успешно инициализирован
 *     ACCESS_DENIED: Функцию может вызывать только SUPERVISOR
 *     OPERATION_FAILED: Невозможно стереть сектора
 *     OPERATION_FAILED: Невозможно прочитать flash-память
 *     OPERATION_FAILED: Невозможно утвердить данные или данные неверны
 *                       (требуется полное форматирование)
 */
void FLASH_INIT(
/* OUT */ RETURN_CODE * return_code);

/*
 * ВЫКЛЮЧЕНИЕ FLASH-драйвера:
 *   return_code: Статус операции
 */
void FLASH_FREE(
/* OUT */ RETURN_CODE * return_code);



/*
 * ГРАНИЦЫ СЕКТОРА:
 *   SECTOR_ID: Номер сектора
 *   pba_start: Адрес начала сектора
 *   pba_end: Адрес конца сектора
 *   return_code: Статус операции
 *     NO_ERROR: Границы получены
 *     INVALID_PARAM: Некоректный номер сектора
 */
void FLASH_SECTOR_BORDERS(
/* IN  */ FLASH_SECTOR_ID SECTOR_ID,
/* OUT */ FLASH_ADDRESS * pba_start,
/* OUT */ FLASH_ADDRESS * pba_end,
/* OUT */ RETURN_CODE * return_code);



/*
 * НОМЕР СЕКТОРА ПО АДРЕСУ: (!!! АЛГОРИТМ НЕ ПРОВЕРЕН !!!)
 *   PBA: Физический адрес в секторе
 *   sector_id: Номер искомого сектора
 *   return_code: Статус операции
 *     NO_ERROR: Номер сектора получен
 *     INVALID_PARAM: Адрес выходит за границы памяти
 *     OPERATION_FAILED: Номер сектора не найден
 */
void FLASH_SECTOR_FIND(
/* IN  */ const FLASH_ADDRESS PBA,
/* OUT */ FLASH_SECTOR_ID * sector_id,
/* OUT */ RETURN_CODE * return_code);


/*
 * ДАННЫЕ СЕКТОРА ПО НОМЕРУ:
 *   SECTOR_ID: Номер получаемого сектора
 *   sector: Структура сектора с данными
 *   return_code: Статус операции
 *     NO_ERROR: Данные сектора получены
 *     INVALID_PARAM: Некоректный номер сектора
 */
void FLASH_SECTOR_SELECT(
/* IN  */ const FLASH_SECTOR_ID SECTOR_ID,
/* OUT */ FLASH_SECTOR_TYPE * sector,
/* OUT */ RETURN_CODE * return_code);



/*
 * СТИРАНИЕ СЕКТОРА:
 *   SECTOR_ID: Номер сектора для стирания
 *   return_code: Код возврата
 *     NO_ERROR: Сектор стерт
 *     INVALID_PARAM: Нокоректный номер сектора
 *     ACCESS_DENIED: Данный сектор невозможно стереть с текущем уровнем доступа
 *     OPERATION_FAILED: Ошибка стирания
 *     OPERATION_FAILED: Ошибка утверждения сектора
 */
void FLASH_SECTOR_ERASE(
/* IN  */ const FLASH_SECTOR_ID SECTOR_ID,
/* OUT */ RETURN_CODE * return_code);



/* ЗАПИСЬ ПРОИЗВОЛЬНЫХ (ВЫРОВНЕННЫХ) ДАННЫХ:
 *   PBA: Физичесий адрес записи (выровнивание по слову)
 *   SIZE: Размер DATA
 *   DATA: Данные для записи (выровнивание по слову)
 *   return_code: Код возврата
 */
void FLASH_WRITE(
/* IN  */ const FLASH_ADDRESS PBA,
/* IN  */ const SIZE32 SIZE,
/* IN  */ const VOID_PTR DATA,
/* OUT */ RETURN_CODE * return_code);



/* ЧТЕНИЕ ПРОИЗВОЛЬНЫХ (ВЫРОВНЕННЫХ) ДАННЫХ:
 *   PBA: Физический адрес чтения (выровнивание по слову)
 *   SIZE: Размер data
 *   data: Данные для чтения (выровнивание по слову)
 *   return_code: Статус операции
 *     NO_ERROR: Данные успешно прочтены
 *     INVALID_PARAM: Неверные параметры (данные не выровнены)
 *     OPERATION_FAILED: Выход за границы адресов
 */
void FLASH_READ(
/* IN  */ const FLASH_ADDRESS PBA,
/* IN  */ const SIZE32 SIZE,
/* OUT */ VOID_PTR data,
/* OUT */ RETURN_CODE * return_code);

#endif /* __FS_FLASH_H__ */
