#ifndef __FS_FTL_H__
#define __FS_FTL_H__

#include "fs_def.h"

/*
 * Индекс логического блока
 */
typedef U32 FTL_INDEX;

/*
 * РЕЖИМ РАБОТЫ:
 *   FTL_MODE_SUPERVISOR: Привелигерованный режим
 *   FTL_MODE_USER: Ограниченный достуа (пользовательский режим)
 */
typedef enum
{
  FTL_MODE_SUPERVISOR,
  FTL_MODE_USER
} FTL_MODE;


/*
 * ИНИЦИАЛИЗАЦИЯ FTL-драйвера:
 *   return_code: Статус операции
 *     NO_ERROR: Успешная инициализация
 *     ACCESS_DENIED: Требуется режим работы суперпользователя
 *     NO_ACTION: Нет доступа к flash-памяти
 *     OPERATION_FAILED: Ошибка чтения блоков
 */
void FTL_INIT(
/* OUT */ RETURN_CODE * return_code);

/*
 * ВЫКЛЮЧЕНИЕ FTL-драйвера:
 *   return_code: Статус операции
 */
void FTL_FREE(
/* OUT */ RETURN_CODE * return_code);

/*
 * ЗАПИСЬ БЛОКА ДАННЫХ:
 *   LBI: Начальный логический номер блока
 *   COUNT: Количество блоков
 *   DATA: Даннные для записи
 *   return_code: Статус операции
 *     NO_ERROR: Успешная запись
 *     INVALID_PARAM: Параметры выходят за границу памяти
 *     OPERATION_FAILED: Ошибка запиши
 */
void FTL_WRITE(
/* IN  */ const FTL_INDEX LBI,
/* IN  */ const SIZE32 COUNT,
/* IN  */ const VOID_PTR DATA,
/* OUT */ RETURN_CODE * return_code);


/*
 * ЧТЕНИЕ БЛОКА ДАННЫХ:
 *   LBI: Начальный логический номер блока
 *   COUNT: Количество блоков
 *   data: Даннные для чтения
 *   return_code: Статус операции
 *     NO_ERROR: Успешное чтение
 *     INVALID_PARAM: Параметры выходят за границу памяти
 *     NO_ACTION: Блока не существует
 *     OPERATION_FAILED: Ошибка чтения
 */
void FTL_READ(
/* IN  */ const FTL_INDEX LBI,
/* IN  */ const SIZE32 COUNT,
/* OUT */ VOID_PTR data,
/* OUT */ RETURN_CODE * return_code);


/*
 * ЗАПУСК СБОРЩИКА МУСОРА (необходимо вызвать перед завершением):
 *   return_code: Статус операции
 */
void FTL_GARBAGE_COLLECT(
/* OUT */ RETURN_CODE * return_code);

#endif /* __FS_FTL_H__ */
