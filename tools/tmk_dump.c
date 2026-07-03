/*
 * tmk_dump — libtidemark wire protocol dump tool
 *
 * Reads a binary wire message from stdin or a file and prints
 * a human-readable representation of the session header, frames,
 * and subrecords.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tidemark/types.h"
#include "../include/tidemark/error.h"
#include "../src/wire/header.h"
#include "../src/wire/frame.h"
#include "../src/wire/subrecord.h"
#include "../src/wire/crc.h"

static const char *frame_type_name(uint16_t ft) {
    switch (ft) {
    case TMK_FRAME_SESSION_OPEN:     return "SESSION_OPEN";
    case TMK_FRAME_SESSION_CLOSE:    return "SESSION_CLOSE";
    case TMK_FRAME_CHANNEL_REGISTER: return "CHANNEL_REGISTER";
    case TMK_FRAME_CHANNEL_BIND:     return "CHANNEL_BIND";
    case TMK_FRAME_PARTITION_CREATE: return "PARTITION_CREATE";
    case TMK_FRAME_SEGMENT_CREATE:   return "SEGMENT_CREATE";
    case TMK_FRAME_SEGMENT_RETIRE:   return "SEGMENT_RETIRE";
    case TMK_FRAME_DATA_INGEST:      return "DATA_INGEST";
    case TMK_FRAME_INDEX_REGISTER:   return "INDEX_REGISTER";
    case TMK_FRAME_CURSOR_OPEN:      return "CURSOR_OPEN";
    case TMK_FRAME_CURSOR_CLOSE:     return "CURSOR_CLOSE";
    case TMK_FRAME_LEASE_ACQUIRE:    return "LEASE_ACQUIRE";
    case TMK_FRAME_LEASE_RELEASE:    return "LEASE_RELEASE";
    case TMK_FRAME_LEASE_BIND:       return "LEASE_BIND";
    case TMK_FRAME_EPOCH_ADVANCE:    return "EPOCH_ADVANCE";
    case TMK_FRAME_RETENTION_TRIGGER:return "RETENTION_TRIGGER";
    case TMK_FRAME_COMPACT_SCHEDULE: return "COMPACT_SCHEDULE";
    case TMK_FRAME_COMPACT_TRIGGER:  return "COMPACT_TRIGGER";
    case TMK_FRAME_CHECKPOINT_CREATE:return "CHECKPOINT_CREATE";
    case TMK_FRAME_CHECKPOINT_RESTORE:return "CHECKPOINT_RESTORE";
    case TMK_FRAME_MIGRATE_BEGIN:    return "MIGRATE_BEGIN";
    case TMK_FRAME_MIGRATE_COMMIT:   return "MIGRATE_COMMIT";
    case TMK_FRAME_MATERIALIZE_EMIT: return "MATERIALIZE_EMIT";
    case TMK_FRAME_MATERIALIZE_SCAN: return "MATERIALIZE_SCAN";
    default:                         return "UNKNOWN";
    }
}

static const char *subrec_kind_name(uint16_t kind) {
    switch (kind) {
    case TMK_SUB_DATA:             return "DATA";
    case TMK_SUB_METADATA:         return "METADATA";
    case TMK_SUB_TOMBSTONE:        return "TOMBSTONE";
    case TMK_SUB_CONTROL:          return "CONTROL";
    case TMK_SUB_INDEX_ENTRY:      return "INDEX_ENTRY";
    case TMK_SUB_LEASE_TOKEN:      return "LEASE_TOKEN";
    case TMK_SUB_CHECKPOINT_REF:   return "CHECKPOINT_REF";
    case TMK_SUB_MIGRATION_DESC:   return "MIGRATION_DESC";
    case TMK_SUB_EPOCH_STATE:      return "EPOCH_STATE";
    case TMK_SUB_RETENTION_POLICY: return "RETENTION_POLICY";
    case TMK_SUB_CAPACITY_INFO:    return "CAPACITY_INFO";
    case TMK_SUB_CURSOR_POS:       return "CURSOR_POS";
    default:                       return "UNKNOWN";
    }
}

int main(int argc, char **argv) {
    FILE *f = stdin;
    if (argc > 1) {
        f = fopen(argv[1], "rb");
        if (!f) { perror("fopen"); return 1; }
    }

    /* Read entire input */
    uint8_t *buf = NULL;
    size_t len = 0;
    size_t cap = 4096;
    buf = (uint8_t *)malloc(cap);
    if (!buf) { fprintf(stderr, "alloc failed\n"); return 1; }

    while (!feof(f)) {
        if (len >= cap) {
            cap *= 2;
            buf = (uint8_t *)realloc(buf, cap);
            if (!buf) { fprintf(stderr, "realloc failed\n"); return 1; }
        }
        size_t n = fread(buf + len, 1, cap - len, f);
        len += n;
        if (n == 0) break;
    }
    if (f != stdin) fclose(f);

    printf("Total bytes: %zu\n\n", len);

    /* Parse session header */
    if (len < TMK_SESSION_HDR_SIZE) {
        printf("ERROR: buffer too short for session header\n");
        free(buf);
        return 1;
    }

    tmk_session_header_t shdr;
    int rc = tmk_parse_session_header(buf, len, &shdr);
    if (rc < 0) {
        printf("ERROR: session header parse failed: %s\n", tmk_error_str(rc));
        free(buf);
        return 1;
    }

    printf("Session Header:\n");
    printf("  magic:       0x%08X %s\n", shdr.magic,
           shdr.magic == TMK_MAGIC ? "(TMK1)" : "(INVALID)");
    printf("  version:     0x%04X\n", shdr.version);
    printf("  session_id:  %u\n", shdr.session_id);
    printf("  epoch:       %u\n", shdr.epoch);
    printf("  frame_count: %u\n", shdr.frame_count);
    printf("  header_crc:  0x%08X\n", shdr.header_crc);
    printf("\n");

    /* Parse frames */
    size_t offset = TMK_SESSION_HDR_SIZE;
    for (uint16_t i = 0; i < shdr.frame_count && offset < len; i++) {
        tmk_parsed_frame_t frame;
        int frc = tmk_parse_single_frame(buf + offset, len - offset, &frame);
        if (frc < 0) {
            printf("Frame %u: PARSE ERROR: %s\n", i, tmk_error_str(frc));
            offset += TMK_FRAME_HDR_SIZE;
            continue;
        }

        printf("Frame %u: %s (0x%04X)\n", i,
               frame_type_name(frame.header.frame_type),
               frame.header.frame_type);
        printf("  length:       %u bytes\n", frame.header.length);
        printf("  sequence:     %u\n", frame.header.sequence);
        printf("  session_id:   %u\n", frame.header.session_id);
        printf("  channel_id:   %u\n", frame.header.channel_id);
        printf("  partition_id: %u\n", frame.header.partition_id);
        printf("  route_id:     %u\n", frame.header.route_id);
        printf("  owner_token:  0x%08X\n", frame.header.owner_token);
        printf("  flags:        0x%04X\n", frame.header.flags);
        printf("  subrecords:   %u\n", frame.parsed_sub_count);

        for (uint16_t j = 0; j < frame.parsed_sub_count; j++) {
            printf("    Sub %u: %s (0x%04X)\n", j,
                   subrec_kind_name(frame.subrecords[j].kind),
                   frame.subrecords[j].kind);
            printf("      length:      %u\n", frame.subrecords[j].length);
            printf("      local_epoch: %u\n", frame.subrecords[j].local_epoch);
            printf("      generation:  %u\n", frame.subrecords[j].generation);
            printf("      lease_token: 0x%08X\n", frame.subrecords[j].lease_token);
        }
        printf("\n");

        offset += (size_t)frc;
    }

    free(buf);
    return 0;
}
