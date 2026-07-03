/*
 * libtidemark — semantic wire validation
 *
 * Validates higher-level protocol constraints that go beyond
 * structural parsing:
 *
 * - Frame sequences must be monotonically increasing
 * - Subrecord combinations must be valid for the frame type
 * - ID ranges must be within configured limits
 * - Epoch values must be monotonically non-decreasing within a session
 * - Owner tokens must be consistent across related frames
 */
#include "validate.h"
#include "../../include/tidemark/error.h"

int tmk_validate_frame_sequence(const tmk_parsed_frame_t *frames, int count) {
    if (!frames || count <= 0) return TMK_ERR_SHORT_BUF;

    for (int i = 1; i < count; i++) {
        if (frames[i].header.sequence <= frames[i-1].header.sequence)
            return TMK_ERR_BAD_SEQUENCE;
    }

    return TMK_OK;
}

int tmk_validate_subrecord_combination(const tmk_parsed_frame_t *frame) {
    if (!frame) return TMK_ERR_SHORT_BUF;

    uint16_t ft = frame->header.frame_type;

    /* Session frames require CONTROL subrecord */
    if (ft == TMK_FRAME_SESSION_OPEN || ft == TMK_FRAME_SESSION_CLOSE) {
        if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
        if (frame->subrecords[0].kind != TMK_SUB_CONTROL)
            return TMK_ERR_BAD_SUBREC_KIND;
    }

    /* Epoch advance requires EPOCH_STATE subrecord */
    if (ft == TMK_FRAME_EPOCH_ADVANCE) {
        if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
        if (frame->subrecords[0].kind != TMK_SUB_EPOCH_STATE)
            return TMK_ERR_BAD_SUBREC_KIND;
    }

    /* Data ingest requires DATA subrecord */
    if (ft == TMK_FRAME_DATA_INGEST) {
        if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
        if (frame->subrecords[0].kind != TMK_SUB_DATA)
            return TMK_ERR_BAD_SUBREC_KIND;
    }

    /* Lease operations require LEASE_TOKEN subrecord */
    if (ft == TMK_FRAME_LEASE_ACQUIRE || ft == TMK_FRAME_LEASE_RELEASE ||
        ft == TMK_FRAME_LEASE_BIND || ft == TMK_FRAME_LEASE_BIND_META) {
        if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
        if (frame->subrecords[0].kind != TMK_SUB_LEASE_TOKEN)
            return TMK_ERR_BAD_SUBREC_KIND;
    }

    /* Segment create requires CAPACITY_INFO */
    if (ft == TMK_FRAME_SEGMENT_CREATE) {
        if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
        if (frame->subrecords[0].kind != TMK_SUB_CAPACITY_INFO)
            return TMK_ERR_BAD_SUBREC_KIND;
    }

    /* Index operations require INDEX_ENTRY */
    if (ft == TMK_FRAME_INDEX_REGISTER || ft == TMK_FRAME_INDEX_BUILD_CROSS ||
        ft == TMK_FRAME_INDEX_BIND_CURSOR) {
        if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
        if (frame->subrecords[0].kind != TMK_SUB_INDEX_ENTRY)
            return TMK_ERR_BAD_SUBREC_KIND;
    }

    /* Cursor operations require CURSOR_POS */
    if (ft == TMK_FRAME_CURSOR_OPEN || ft == TMK_FRAME_MATERIALIZE_EMIT) {
        if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
        if (frame->subrecords[0].kind != TMK_SUB_CURSOR_POS)
            return TMK_ERR_BAD_SUBREC_KIND;
    }

    /* Checkpoint operations require CHECKPOINT_REF */
    if (ft == TMK_FRAME_CHECKPOINT_CREATE || ft == TMK_FRAME_CHECKPOINT_RESTORE) {
        if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
        if (frame->subrecords[0].kind != TMK_SUB_CHECKPOINT_REF)
            return TMK_ERR_BAD_SUBREC_KIND;
    }

    /* Migration operations require MIGRATION_DESC */
    if (ft == TMK_FRAME_MIGRATE_BEGIN || ft == TMK_FRAME_MIGRATE_TRANSFER ||
        ft == TMK_FRAME_MIGRATE_COMMIT) {
        if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
        if (frame->subrecords[0].kind != TMK_SUB_MIGRATION_DESC)
            return TMK_ERR_BAD_SUBREC_KIND;
    }

    /* Retention config requires RETENTION_POLICY */
    if (ft == TMK_FRAME_RETENTION_CONFIG) {
        if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
        if (frame->subrecords[0].kind != TMK_SUB_RETENTION_POLICY)
            return TMK_ERR_BAD_SUBREC_KIND;
    }

    return TMK_OK;
}

int tmk_validate_id_ranges(const tmk_parsed_frame_t *frame) {
    if (!frame) return TMK_ERR_SHORT_BUF;

    /* Check IDs are within valid ranges */
    if (frame->header.channel_id > TMK_MAX_CHANNELS * 4)
        return TMK_ERR_INVALID_ID;
    if (frame->header.partition_id > TMK_MAX_PARTITIONS * 4)
        return TMK_ERR_INVALID_ID;
    if (frame->header.route_id > TMK_MAX_SEGMENTS * 4)
        return TMK_ERR_INVALID_ID;

    return TMK_OK;
}

int tmk_validate_epoch_ordering(const tmk_parsed_frame_t *frames, int count) {
    if (!frames || count <= 0) return TMK_ERR_SHORT_BUF;

    uint32_t last_epoch = 0;
    for (int i = 0; i < count; i++) {
        for (uint16_t j = 0; j < frames[i].parsed_sub_count; j++) {
            if (frames[i].subrecords[j].local_epoch > 0) {
                if (frames[i].subrecords[j].local_epoch < last_epoch)
                    return TMK_ERR_BAD_EPOCH;
                last_epoch = frames[i].subrecords[j].local_epoch;
            }
        }
    }

    return TMK_OK;
}

int tmk_validate_owner_consistency(const tmk_parsed_frame_t *frames, int count) {
    if (!frames || count <= 0) return TMK_ERR_SHORT_BUF;

    /* Track owner tokens per partition */
    struct { uint16_t part_id; uint32_t token; int set; } owners[TMK_MAX_PARTITIONS];
    int owner_count = 0;

    for (int i = 0; i < count; i++) {
        uint16_t part_id = frames[i].header.partition_id;
        uint32_t token = frames[i].header.owner_token;
        if (part_id == 0 || token == 0) continue;

        /* Find existing owner record */
        int found = -1;
        for (int j = 0; j < owner_count; j++) {
            if (owners[j].part_id == part_id) { found = j; break; }
        }

        if (found >= 0) {
            if (owners[found].token != token)
                return TMK_ERR_BAD_OWNER;
        } else if (owner_count < TMK_MAX_PARTITIONS) {
            owners[owner_count].part_id = part_id;
            owners[owner_count].token = token;
            owners[owner_count].set = 1;
            owner_count++;
        }
    }

    return TMK_OK;
}
