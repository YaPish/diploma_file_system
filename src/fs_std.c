#include "fs_def.h"
#include "fs_std.h"

void STD_MEMCPY(
/* IN  */ const SIZE32 SIZE,
/* IN  */ const VOID_PTR SRC,
/* OUT */ VOID_PTR dest)
{
  for(register SIZE32 i = 0UL; i < SIZE; i++)
  {
    ((U8 *)dest)[i] = ((U8 *)SRC)[i];
  }
}

void STD_MEMSET(
/* IN  */ const SIZE32 SIZE,
/* IN  */ const U8 VALUE,
/* OUT */ VOID_PTR dest)
{
  for(register SIZE32 i = 0UL; i < SIZE; i++)
  {
    ((U8 *)dest)[i] = VALUE;
  }
}

void STD_STRCMP(
/* IN  */ const CHAR * FIRST,
/* IN  */ const CHAR * SECOND,
/* OUT */ I32 * result)
{
  CHAR * m_first = (CHAR *)FIRST;
  CHAR * m_second = (CHAR *)SECOND;
  while((*m_first != '\0') && (*m_first == *m_second))
  {
    m_first++;
    m_second++;
  }
  *result = (I32)(*(const U8 *)m_first) - (I32)(*(const U8*)m_second);
}

void STD_STRNCPY(
/* IN  */ const SIZE32 SIZE,
/* IN  */ const CHAR * SRC,
/* OUT */ CHAR * dest)
{
  for(register SIZE32 i = 0UL; i < SIZE; i++)
  {
    dest[i] = SRC[i];
    if(SRC[i] == '\0')
    {
      break;
    }
  }
}
