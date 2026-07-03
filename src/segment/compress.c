/*
 * libtidemark — segment data compression
 *
 * Run-length encoding (RLE) with literal fallback for compressing
 * time-series segment data during compaction.  The encoding
 * switches between two modes:
 *
 * Run mode:   byte[0] = count (1-254), byte[1] = value
 *             Used when 3+ consecutive bytes have the same value.
 *
 * Literal mode: byte[0] = 0xFF, byte[1] = count, byte[2..N+1] = data
 *               Used for non-repeating sequences.
 *
 * XOR delta encoding computes byte-wise XOR between consecutive
 * values, then RLE-encodes the result.  For time-series data where
 * consecutive samples are similar, XOR deltas produce many zeros
 * which compress well with RLE.
 */
#include "compress.h"
#include <string.h>

size_t tmk_rle_worst_case(size_t in_len) {
    /* Worst case: all literals → (0xFF + count + data) per 254 bytes */
    return in_len + (in_len / 254 + 1) * 2;
}

int tmk_rle_encode(const uint8_t *in, size_t in_len,
                   uint8_t *out, size_t out_len) {
    if (!in || !out || in_len == 0) return 0;

    size_t ri = 0, wi = 0;

    while (ri < in_len && wi < out_len) {
        /* Count run length */
        uint8_t val = in[ri];
        size_t run = 1;
        while (ri + run < in_len && in[ri + run] == val && run < 254) {
            run++;
        }

        if (run >= 3) {
            /* Encode as run */
            if (wi + 2 > out_len) break;
            out[wi++] = (uint8_t)run;
            out[wi++] = val;
            ri += run;
        } else {
            /* Collect literals */
            size_t lit_start = ri;
            size_t lit_count = 0;

            while (ri < in_len && lit_count < 254) {
                /* Check if next sequence is a run */
                if (ri + 3 <= in_len &&
                    in[ri] == in[ri+1] && in[ri+1] == in[ri+2]) {
                    break;
                }
                ri++;
                lit_count++;
            }

            if (lit_count > 0) {
                if (wi + 2 + lit_count > out_len) break;
                out[wi++] = 0xFF;
                out[wi++] = (uint8_t)lit_count;
                memcpy(out + wi, in + lit_start, lit_count);
                wi += lit_count;
            }
        }
    }

    return (int)wi;
}

int tmk_rle_decode(const uint8_t *in, size_t in_len,
                   uint8_t *out, size_t out_len) {
    if (!in || !out || in_len == 0) return 0;

    size_t ri = 0, wi = 0;

    while (ri < in_len && wi < out_len) {
        uint8_t marker = in[ri++];

        if (marker == 0xFF) {
            /* Literal block */
            if (ri >= in_len) break;
            uint8_t count = in[ri++];
            if (ri + count > in_len) break;
            if (wi + count > out_len) break;
            memcpy(out + wi, in + ri, count);
            wi += count;
            ri += count;
        } else {
            /* Run */
            if (ri >= in_len) break;
            uint8_t count = marker;
            uint8_t val = in[ri++];
            if (wi + count > out_len) break;
            memset(out + wi, val, count);
            wi += count;
        }
    }

    return (int)wi;
}

int tmk_xor_delta_encode(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_len) {
    if (!in || !out || in_len == 0) return 0;
    if (out_len < in_len) return -1;

    /* First byte is literal */
    out[0] = in[0];
    for (size_t i = 1; i < in_len; i++) {
        out[i] = in[i] ^ in[i-1];
    }

    return (int)in_len;
}

int tmk_xor_delta_decode(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_len) {
    if (!in || !out || in_len == 0) return 0;
    if (out_len < in_len) return -1;

    out[0] = in[0];
    for (size_t i = 1; i < in_len; i++) {
        out[i] = in[i] ^ out[i-1];
    }

    return (int)in_len;
}
