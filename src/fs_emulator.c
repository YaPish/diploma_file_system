#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "fs_def.h"
#include "fs_std.h"
#include "fs_emulator.h"

I16  g_flash_fd = -1;
U8 * g_flash_mem = (VOID_PTR)(0);

void EMULATOR_INIT(
/* IN  */ const CHAR * FLASH_NAME,
/* OUT */ RETURN_CODE * return_code)
{
  g_flash_fd = open(FLASH_NAME, O_RDWR | O_CREAT, 0644);
  if(g_flash_fd < 0)
  {
    *return_code = OPERATION_FAILED;
    return;
  }

  // Устанавливаем размер файла
  if(ftruncate(g_flash_fd, FLASH_SIZE) < 0)
  {
    close(g_flash_fd);

    *return_code = OPERATION_FAILED;
    return;
  }

  // Отображаем файл в память
  g_flash_mem
    = mmap((VOID_PTR)(0), FLASH_SIZE,
      PROT_READ | PROT_WRITE, MAP_SHARED, g_flash_fd, 0);
  if(g_flash_mem == MAP_FAILED)
  {
    close(g_flash_fd);

    *return_code = OPERATION_FAILED;
    return;
  }

  *return_code = NO_ERROR;
}

void EMULATOR_FREE(void)
{
  if(g_flash_mem)
  {
    munmap(g_flash_mem, FLASH_SIZE);
    g_flash_mem = (VOID_PTR)(0);
  }
  if(g_flash_fd >= 0)
  {
    close(g_flash_fd);
    g_flash_fd = -1;
  }
}
