/*
 * libtidemark — CRC-32C (Castagnoli) implementation
 *
 * Uses the iSCSI polynomial 0x1EDC6F41 with standard bit-reversal.
 * Software table-driven approach — no hardware intrinsics required.
 */
#include "crc.h"

static uint32_t crc_table[256];
static int table_ready = 0;

static void build_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0x82F63B78u;
            else
                crc >>= 1;
        }
        crc_table[i] = crc;
    }
    table_ready = 1;
}

uint32_t tmk_crc32c_update(uint32_t crc, const uint8_t *data, size_t len) {
    if (!table_ready) build_table();
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

uint32_t tmk_crc32c(const uint8_t *data, size_t len) {
    return tmk_crc32c_update(0, data, len);
}
