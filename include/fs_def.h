#ifndef __FS_DEF_H__
#define __FS_DEF_H__

/*
 * Значение не имеет смысла
 */
#define UN_SET 0xFFFFFFFFU

typedef char               CHAR;
typedef signed char        I8;
typedef unsigned char      U8;
typedef signed short       I16;
typedef unsigned short     U16;
typedef signed int         I32;
typedef unsigned int       U32;
typedef float              F32;

typedef U32 SIZE32;
typedef I32 SSIZE32;

typedef void * VOID_PTR;

typedef enum {
  NO_ERROR,
  NO_ACTION,
  OPERATION_FAILED,
  INVALID_PARAM,
  ACCESS_DENIED,
  DEVICE_BUSY
} RETURN_CODE;

#endif /* __FS_DEF_H__ */
