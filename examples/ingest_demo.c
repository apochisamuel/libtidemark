/*
 * libtidemark — ingestion demo
 *
 * Demonstrates basic time-series data ingestion through the
 * wire protocol.  Creates a session, registers a channel,
 * creates a partition and segment, ingests data, and queries
 * through cursor materialization.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tidemark/tidemark.h"
#include "../src/wire/serialize.h"

int main(void) {
    tmk_context_t *ctx = tmk_context_create();
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    /* Build a wire message */
    uint8_t msg[4096];
    tmk_serializer_t ser;
    tmk_serializer_init(&ser, msg, sizeof(msg), /*session_id=*/1, /*epoch=*/1);

    /* Frame 1: SESSION_OPEN */
    tmk_serializer_begin_frame(&ser, TMK_FRAME_SESSION_OPEN,
                               0, 0, 0, 0, 0);
    tmk_serializer_add_subrecord(&ser, TMK_SUB_CONTROL, 1, 1, 0, NULL, 0);
    tmk_serializer_end_frame(&ser);

    /* Frame 2: CHANNEL_REGISTER */
    tmk_serializer_begin_frame(&ser, TMK_FRAME_CHANNEL_REGISTER,
                               /*ch=*/1, 0, 0, 0, 0);
    tmk_serializer_add_subrecord(&ser, TMK_SUB_CONTROL, 1, 1, 0, NULL, 0);
    tmk_serializer_end_frame(&ser);

    /* Frame 3: PARTITION_CREATE */
    tmk_serializer_begin_frame(&ser, TMK_FRAME_PARTITION_CREATE,
                               0, /*part=*/1, 0, /*owner=*/0xABCD, 0);
    tmk_serializer_add_subrecord(&ser, TMK_SUB_CONTROL, 1, 1, 0, NULL, 0);
    tmk_serializer_end_frame(&ser);

    /* Frame 4: SEGMENT_CREATE */
    uint8_t cap_payload[4] = {0x00, 0x10, 0x00, 0x00}; /* 4096 */
    tmk_serializer_begin_frame(&ser, TMK_FRAME_SEGMENT_CREATE,
                               0, /*part=*/1, /*seg=*/1, /*owner=*/0xABCD, 0);
    tmk_serializer_add_subrecord(&ser, TMK_SUB_CAPACITY_INFO, 1, 1, 0,
                                 cap_payload, 4);
    tmk_serializer_end_frame(&ser);

    /* Frame 5: CHANNEL_BIND */
    tmk_serializer_begin_frame(&ser, TMK_FRAME_CHANNEL_BIND,
                               /*ch=*/1, /*part=*/1, 0, /*owner=*/0xABCD, 0);
    tmk_serializer_add_subrecord(&ser, TMK_SUB_CONTROL, 1, 1, 0, NULL, 0);
    tmk_serializer_end_frame(&ser);

    /* Frame 6: DATA_INGEST */
    uint8_t data[] = "Hello, libtidemark!";
    tmk_serializer_begin_frame(&ser, TMK_FRAME_DATA_INGEST,
                               /*ch=*/1, /*part=*/1, /*seg=*/1, /*owner=*/0xABCD, 0);
    tmk_serializer_add_subrecord(&ser, TMK_SUB_DATA, 1, 1, 0,
                                 data, (uint16_t)strlen((char*)data));
    tmk_serializer_end_frame(&ser);

    tmk_serializer_finalize(&ser);

    /* Process the message */
    int rc = tmk_channel_ingest(ctx, msg, tmk_serializer_size(&ser));
    printf("Ingest result: %d frames processed\n", rc);

    tmk_context_destroy(ctx);
    printf("Demo complete.\n");
    return 0;
}
