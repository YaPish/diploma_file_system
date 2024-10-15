#ifndef __FS_DEF_H__
#define __FS_DEF_H__

typedef signed char        i8;
typedef unsigned char      u8;
typedef signed short       i16;
typedef unsigned short     u16;
typedef signed int         i32;
typedef unsigned int       u32;
typedef signed long long   s64;
typedef unsigned long long u64;
typedef float              f32;
typedef double             f64;

typedef enum {
  NO_ERROR,
  NO_ACTION,
  INVALID_PARAM
} ret_code;

#endif /* __FS_DEF_H__ */
