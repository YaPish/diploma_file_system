#ifndef __FS_DRIVER_H__
#define __FS_DRIVER_H__

#include "fs_def.h"

/*
 * Размер имени файла
 */
#define FILE_NAME_SIZE 50U

/*
 * Размер имени тэга
 */
#define TAG_NAME_SIZE 19U

/*
 * Количество тэгов
 */
#define FS_TAGS_COUNT 52U



/*
 * Индекс файла
 */
typedef U16 FILE_ID;

/*
 * Имя файла
 */
typedef CHAR FILE_NAME[FILE_NAME_SIZE];

/*
 * Позиция в файле
 */
typedef I32 FILE_POSITION;

/*
 * Имя тэга
 */
typedef CHAR TAG_NAME[TAG_NAME_SIZE];

/*
 * Битовая карта тэгов (55 тэга)
 */
typedef U8 TAG_BITMAP[7U];

/*
 * РЕЖИМ ОТКРЫТИЯ ФАЙЛА:
 *   FILE_MODE_READ_ONLY: Только на чтение
 *   FILE_MODE_READ_WRITE: Чтение и запись
 */
typedef enum
{
  FILE_MODE_READ_ONLY  = 0x01,
  FILE_MODE_READ_WRITE = 0x02
} FILE_MODE;

/*
 * ПОЗИЦИЯ В ФАЙЛЕ:
 *   FILE_SEEK_SET: Отсчет от начала
 *   FILE_SEEK_CUR: Отсчет с текущей позиции
 *   FILE_SEEK_END: Отсчет с конца
 */
typedef enum
{
  FILE_SEEK_SET = 0x01,
  FILE_SEEK_CUR = 0x02,
  FILE_SEEK_END = 0x03
} FILE_SEEK;

/*
 * ОШИБКИ ФС:
 *   FILE_ERROR_PERMISSION: Операция не разрешена
 *   FILE_ERROR_NO_FILE: Файла не существует
 *   FILE_ERROR_IO: Ошибка ввода/вывода
 *   FILE_ERROR_DESCRIPTOR: Неправильный дескриптор файла
 *   FILE_ERROR_BUSY: Файл занят
 *   FILE_ERROR_EXIST: Файл уже существует
 *   FILE_ERROR_INVALID_PARAM: Неправильный аргумент
 *   FILE_ERROR_NAME_SIZE: Неверная длина имени
 *   FILE_ERROR_FILE_SIZE: Размер файла слишком большой
 *   FILE_ERROR_OVERFLOW: Текущая позиция файла выходит за границы
 *   FILE_ERROR_NO_SPACE: Места в памяти нет
 */
typedef enum
{
  FILE_ERROR_PERMISSION    = 0x01,
  FILE_ERROR_NO_FILE       = 0x02,
  FILE_ERROR_IO            = 0x03,
  FILE_ERROR_DESCRIPTOR    = 0x04,
  FILE_ERROR_BUSY          = 0x05,
  FILE_ERROR_EXIST         = 0x06,
  FILE_ERROR_INVALID_PARAM = 0x07,
  FILE_ERROR_NAME_SIZE     = 0x08,
  FILE_ERROR_FILE_SIZE     = 0x09,
  FILE_ERROR_OVERFLOW      = 0x0A,
  FILE_ERROR_NO_SPACE      = 0x0B
} FILE_ERROR;

/*
 * СТАТУС ОТКРЫТОГО ФАЙЛА:
 *   size: Общий размер файла
 *   position: Позиция в файле (в байтах)
 *   mode: Режим открытия
 *   tags: Битовая карта тэгов
 *   (16 байт + enum = 20 байт)
 */
typedef struct
{
  SIZE32         size;
  FILE_POSITION  position;
  FILE_MODE      mode;
  TAG_BITMAP     tags;
} FILE_STATUS_TYPE;

/*
 * СОЗДАНИЕ ФАЙЛА:
 *   NAME: Имя файла
 *   return_code: Статус операции
 *   file_erro: Ошибка ФС
 */
void FS_FILE_CREATE(
/* IN  */ const FILE_NAME NAME,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error);

/*
 * ОТКРЫТИЕ ФАЙЛА:
 *   NAME: Имя файла
 *   MODE: Режим открытия
 *   id: Дескриптор файла
 *   return_code: Статус операции
 *   file_error: Ошибка ФС
 */
void FS_FILE_OPEN(
/* IN  */ const FILE_NAME NAME,
/* IN  */ const FILE_MODE MODE,
/* OUT */ FILE_ID * id,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error);

/*
 * ЗАКРЫТИЕ ФАЙЛА:
 *   ID: Дескриптор файла
 *   return_code: Статус операции
 *   file_error: Ошибка ФС
 */
void FS_FILE_CLOSE(
/* IN  */ const FILE_ID ID,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error);

/*
 * ЧТЕНИЕ ИЗ ФАЙЛА:
 *   ID: Дескриптор файла
 *   IN_LENGTH: Заданное количество байт на чтение
 *   out_length: Прочитанное количество байт
 *   data: Данные
 *   return_code: Статус операции
 *   file_error: Ошибка ФС
 */
void FS_FILE_READ(
/* IN  */ const FILE_ID ID,
/* IN  */ const SIZE32 IN_LENGTH,
/* OUT */ SIZE32 * out_length,
/* OUT */ VOID_PTR data,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error);

/*
 * ЗАПИСЬ В ФАЙЛ:
 *   ID: Дескриптор файла
 *   LENGTH: Заданное количество байт на запись
 *   DATA: Данные
 *   return_code: Статус операции
 *   file_error: Ошибка ФС
 */
void FS_FILE_WRITE(
/* IN  */ const FILE_ID ID,
/* IN  */ const SIZE32 LENGTH,
/* IN  */ const VOID_PTR DATA,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error);

/*
 * УСТАНОВКА ПОЗИЦИИ В ФАЙЛЕ:
 *   ID: Дескриптор файла
 *   OFFSET: Смещение
 *   WHENCE: Начала отсчета
 *   position: смещение в байтах от начала файла
 *   return_code: Статус операции
 *   file_error: Ошибка ФС
 */
void FS_FILE_SEEK(
/* IN  */ const FILE_ID ID,
/* IN  */ const FILE_POSITION OFFSET,
/* IN  */ const FILE_SEEK WHENCE,
/* OUT */ FILE_POSITION * position,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error);

/*
 * УДАЛЕНИЕ ФАЙЛА:
 *   NAME: Имя файла
 *   return_code: Статус операции
 *   file_error: Ошибка ФС
 */
void FS_FILE_REMOVE(
/* IN  */ const FILE_NAME NAME,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error);

/*
 * ПЕРЕИМЕНОВАНИЕ ФАЙЛА:
 *   OLD_NAME: Старое имя файла
 *   NEW_NAME: Новое имя файла
 *   return_code: Статус операции
 *   file_error: Ошибка ФС
 */
void FS_FILE_RENAME(
/* IN  */ const FILE_NAME OLD_NAME,
/* IN  */ const FILE_NAME NEW_NAME,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error);

/*
 * ПОЛУЧЕНИЕ СТАТУСА ФАЙЛА:
 *   ID: Дескриптор файла
 *   status: Статус файла
 *   return_code: Статус операции
 *   file_error: Ошибка ФС
 */
void FS_FILE_STATUS(
/* IN  */ const FILE_ID ID,
/* OUT */ FILE_STATUS_TYPE * status,
/* OUT */ RETURN_CODE * return_code,
/* OUT */ FILE_ERROR * file_error);



/*
 * ДОБАВИТЬ ТЕГ К ФАЙЛУ:
 *   NAME: Имя файла
 *   TAG: Название тега
 *   return_code: Статус операции
 */
void FS_TAG_ADD(
/* IN  */ const FILE_NAME NAME,
/* IN  */ const TAG_NAME TAG,
/* IN  */ RETURN_CODE * return_code);

/*
 * УДАЛИТЬ ТЕГ У ФАЙЛА:
 *   NAME: Имя файла
 *   TAG: Название тега
 *   return_code: Статус операции
 */
void FS_TAG_REMOVE(
/* IN  */ const FILE_NAME NAME,
/* IN  */ const TAG_NAME TAG,
/* OUT */ RETURN_CODE * return_code);

/*
 * ПЕРЕИМЕНОВАТЬ ТЕГ В СИСТЕМЕ:
 *   OLD_NAME: Старое название
 *   NEW_NAME: Новое название
 *   return_code: Статус операции
 */
void FS_TAG_RENAME(
/* IN  */ const TAG_NAME OLD_NAME,
/* IN  */ const TAG_NAME NEW_NAME,
/* OUT */ RETURN_CODE * return_code);



/*
 * ИНИЦИАЛИЗАЦИЯ ФС
 */
void FS_INIT(RETURN_CODE* return_code);

/*
 * ОТКЛЮЧЕНИЕ ФС
 */
void FS_FREE(RETURN_CODE * return_code);

/* TODO: search by tags */

#endif
