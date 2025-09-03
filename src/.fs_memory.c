#include "fs_def.h"
#include "fs_memory.h"

/* Инициализация файловой системы */
void FS_INIT(
/* IN  */ const char * PATH,
/* OUT */ FILESYSTEM * fs,
/* OUT */ RETURN_CODE_TYPE * return_code) {
  *return_code = NO_ERROR;

  fs->block_map_size = 1024ULL;
  fs->block_size = 4096ULL;

  fs->block_map = malloc(fs->block_map_size); // TODO: kalloc (kmalloc)

#ifdef __linux__
  const struct block_device *BDEV
    = blkdev_get_by_path(PATH, FMODE_READ | FMODE_WRITE, NULL);
  if(IS_ERR(BDEV)) {
    *return_code = NO_ACTION;
    return;
  }
  fs->device = bdev;

#elif defined(__APPLE__)
  const I16 FD = open(PATH, O_RDWR);
  if(FD == -1) {
    *return_code = NO_ACTION;
    return;
  }
  fs->device = (VOID_PTR)(INT64_PTR)FD;

#endif
}

void FS_FREE(
/* IN  */ FILESYSTEM * fs,
/* OUT */ RETURN_CODE_TYPE * return_code) {
  *return_code = NO_ERROR;

  free(fs->block_map); // TODO: kfree

#ifdef __linux__
  struct block_device *bdev = (struct block_device *)fs->device;
  (void)blkdev_put(bdev, FMODE_READ | FMODE_WRITE);

#elif defined(__APPLE__)
  (void)close((I16)(INT64_PTR)fs->device);

#endif
}

/* Выделение блока */
void FS_BLOCK_ALLOCATE(
/* INOUT */ FILESYSTEM * fs,
/* OUT   */ BLOCK * block_id,
/* OUT   */ RETURN_CODE_TYPE * return_code) {
  *return_code = NO_ERROR;
  for(SIZE64 i = 0ULL; i < fs->block_map_size; i++) {
    if(!fs->block_map[i]) {
      fs->block_map[i] = 1U;
      *block_id = i;
      return;
    }
  }
  *return_code = NO_ACTION;
}

/* Освобождение блока */
void FS_BLOCK_FREE(
/* INOUT */ FILESYSTEM * fs,
/* IN    */ const BLOCK BLOCK_ID,
/* OUT   */ RETURN_CODE_TYPE * return_code) {
  *return_code = NO_ERROR;
  if(BLOCK_ID >= fs->block_map_size) {
    *return_code = INVALID_PARAM;
  }
  fs->block_map[BLOCK_ID] = 0U;
}

/* Запись данных в блок */
void FS_BLOCK_WRITE(
/* INOUT */ FILESYSTEM * fs,
/* IN    */ const BLOCK BLOCK_ID,
/* IN    */ const SIZE64 SIZE,
/* IN    */ const VOID_PTR DATA,
/* OUT   */ RETURN_CODE_TYPE * return_code) {
  *return_code = NO_ERROR;
  if(!((BLOCK_ID < fs->block_map_size) && (SIZE <= fs->block_size))) {
    *return_code = INVALID_PARAM;
  }

#ifdef __linux__
  struct block_device *bdev = (struct block_device *)device->platform_data;
  struct bio *bio = bio_alloc(GFP_KERNEL, 1);
  if(!bio) {
    *return_code = NO_ACTION;
    return;
  }

  bio->bi_bdev = bdev;
  bio->bi_iter.bi_sector = BLOCK_ID * (fs->block_size / 512ULL);
  bio->bi_opf = REQ_OP_WRITE;
  (void)bio_add_page(bio, virt_to_page(data), size, offset_in_page(data));

  (void)submit_bio_wait(bio);
  (void)bio_put(bio);

#elif defined(__APPLE__)
  const I16 FILE_DESCRIPTOR = (I16)(*(I16 *)fs->device);
  const SIZE64 OFFSET = BLOCK_ID * fs->block_size;
  const SSIZE64 WR_BYTES = pwrite(FILE_DESCRIPTOR, DATA, SIZE, OFFSET);
  if(WR_BYTES != SIZE) {
    *return_code = NO_ACTION;
  }

#endif
}

/* Чтение данных из блока */
void FS_BLOCK_READ(
/* INOUT */ FILESYSTEM * fs,
/* IN    */ const BLOCK BLOCK_ID,
/* IN    */ const SIZE64 SIZE,
/* OUT   */ VOID_PTR * buffer,
/* OUT   */ RETURN_CODE_TYPE * return_code) {
  *return_code = NO_ERROR;
  if(!((BLOCK_ID < fs->block_map_size) && (SIZE <= fs->block_size))) {
    *return_code = INVALID_PARAM;
  }

#ifdef __linux__
  struct block_device * bdev = (struct block_device *)fs->device;
  struct bio * bio = bio_alloc(GFP_KERNEL, 1);
  if(!bio) {
    *return_code = NO_ACTION;
    return;
  }

  bio->bi_bdev = bdev;
  bio->bi_iter.bi_sector = BLOCK_ID * (fs->block_size / 512ULL);
  bio->bi_opf = REQ_OP_READ;
  (void)bio_add_page(bio, virt_to_page(buffer), SIZE, offset_in_page(buffer));

  (void)submit_bio_wait(bio);
  (void)bio_put(bio);

#elif defined(__APPLE__)
  const I16 FILE_DESCRIPTOR = (I16)(*(I16 *)fs->device);
  const SIZE64 OFFSET = BLOCK_ID * fs->block_size;
  const SSIZE64 RD_BYTES = pread(FILE_DESCRIPTOR, buffer, SIZE, OFFSET);
  if(RD_BYTES != SIZE) {
    *return_code = NO_ACTION;
  }

#endif
}
