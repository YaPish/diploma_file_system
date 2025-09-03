#include "fs_def.h"
#include "fs_crypt.h"

void CRYPT_XOR(
/* INOUT */ VOID_PTR data,
/* IN    */ const SIZE32 SIZE,
/* IN    */ const U32 ADDRESS)
{
  const U8 M_IV_SIZE = 8U;
  const U8 M_IV[] = {
    (ADDRESS >> (0 * 4)) & 0xFF,
    (ADDRESS >> (1 * 4)) & 0xFF,
    (ADDRESS >> (2 * 4)) & 0xFF,
    (ADDRESS >> (3 * 4)) & 0xFF,
    (ADDRESS >> (4 * 4)) & 0xFF,
    (ADDRESS >> (5 * 4)) & 0xFF,
    (ADDRESS >> (6 * 4)) & 0xFF,
    (ADDRESS >> (7 * 4)) & 0xFF
  };

  const U8 M_KEY_SIZE = 16U;
  U8 m_key[] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
  };

  for(register U8 i = 0U; i < M_KEY_SIZE; i++)
  {
    m_key[i] ^= M_IV[i % 8U];
  }

  // Циклическое шифрование
  for(register U8 i = 0U; i < SIZE; i++)
  {
    if(((U8 *)data)[i] == 0xFF)
    {
      continue;
    }
    ((U8 *)data)[i] ^= m_key[i % M_KEY_SIZE];
    m_key[i % M_KEY_SIZE] = (m_key[i % M_KEY_SIZE] << 1U) |
                            (m_key[i % M_KEY_SIZE] >> 7U);
  }
}

void HASH_CRC(
/* IN  */ const VOID_PTR DATA,
/* IN  */ const SIZE32 SIZE,
/* OUT */ U32 * crc)
{
  const U32 CRC32_POLYNOMIAL = 0x04C11DB7;
  const U32 CRC32_INITIAL    = 0xFFFFFFFF;
  const U32 CRC32_FINAL_XOR  = 0xFFFFFFFF;

  U32 m_crc = CRC32_INITIAL;
  for(register SIZE32 i = 0UL; i < SIZE; i++)
  {
    m_crc ^= ((U8 *)DATA)[i];

    // Развернутый цикл
    m_crc = (m_crc & 1) ? ((m_crc >> 1) ^ CRC32_POLYNOMIAL) : (m_crc >> 1);
    m_crc = (m_crc & 1) ? ((m_crc >> 1) ^ CRC32_POLYNOMIAL) : (m_crc >> 1);
    m_crc = (m_crc & 1) ? ((m_crc >> 1) ^ CRC32_POLYNOMIAL) : (m_crc >> 1);
    m_crc = (m_crc & 1) ? ((m_crc >> 1) ^ CRC32_POLYNOMIAL) : (m_crc >> 1);
    m_crc = (m_crc & 1) ? ((m_crc >> 1) ^ CRC32_POLYNOMIAL) : (m_crc >> 1);
    m_crc = (m_crc & 1) ? ((m_crc >> 1) ^ CRC32_POLYNOMIAL) : (m_crc >> 1);
    m_crc = (m_crc & 1) ? ((m_crc >> 1) ^ CRC32_POLYNOMIAL) : (m_crc >> 1);
    m_crc = (m_crc & 1) ? ((m_crc >> 1) ^ CRC32_POLYNOMIAL) : (m_crc >> 1);
  }

  *crc = m_crc ^ CRC32_FINAL_XOR;
}
