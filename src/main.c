#include "fs_def.h"
#include "fs_std.h"
#include "fs_emulator.h"
#include "fs_ftl.h"
#include "fs_driver.h"

int main(void) {
  RETURN_CODE m_emulator_error = NO_ERROR;
  EMULATOR_INIT("flash.bin", &m_emulator_error);
  if(NO_ERROR != m_emulator_error)
  {
    return -1;
  }

  RETURN_CODE m_fs_error = NO_ERROR;
  FS_INIT(&m_fs_error);


  FS_FREE(&m_fs_error);

  EMULATOR_FREE();

  return 0;
  /*
  if(NO_ERROR == m_fs_error)
  {
    return 0;
  }
  else
  {
    return -2;
  }
  */
}
