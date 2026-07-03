#ifndef TMK_WIRE_CRC_H
#define TMK_WIRE_CRC_H

#include <stdint.h>
#include <stddef.h>

uint32_t tmk_crc32c(const uint8_t *data, size_t len);
uint32_t tmk_crc32c_update(uint32_t crc, const uint8_t *data, size_t len);

#endif
