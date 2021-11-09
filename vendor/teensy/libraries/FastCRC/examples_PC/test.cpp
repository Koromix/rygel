#include <stdio.h>
#include "FastCRC.h"

// g++ -std=gnu++11 test.cpp FastCRCsw.cpp -otest.exe

FastCRC32 CRC32;
uint8_t buf[9] = {'1','2','3','4','5','6','7','8','9'};


int main()
{

  uint32_t crc;

  crc = CRC32.crc32(buf, sizeof(buf));
  printf("crc32 %s\n", 0xcbf43926 == crc ? "is OK" : "is NOT OK");

  crc = CRC32.cksum(buf, sizeof(buf));
  printf("cksum %s\n", 0x765e7680 == crc ? "is OK" : "is NOT OK");

}