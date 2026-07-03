/*
 * libtidemark — error code string conversion
 */
#include <stddef.h>
#include "../../include/tidemark/error.h"

static const struct { int code; const char *msg; } error_table[] = {
    { TMK_OK,                      "success" },
    { TMK_ERR_SHORT_BUF,           "buffer too short" },
    { TMK_ERR_BAD_MAGIC,           "invalid magic number" },
    { TMK_ERR_BAD_VERSION,         "unsupported protocol version" },
    { TMK_ERR_BAD_CRC,             "CRC mismatch" },
    { TMK_ERR_BAD_FRAME_LEN,       "invalid frame length" },
    { TMK_ERR_BAD_SUBREC_LEN,      "invalid subrecord length" },
    { TMK_ERR_NOT_FOUND,          "no active session" },
    { TMK_ERR_SESSION_FULL,        "session table full" },
    { TMK_ERR_CHANNEL_FULL,        "channel table full" },
    { TMK_ERR_PARTITION_FULL,      "partition table full" },
    { TMK_ERR_SEGMENT_FULL,        "segment table full" },
    { TMK_ERR_CURSOR_FULL,         "cursor table full" },
    { TMK_ERR_LEASE_FULL,          "lease table full" },
    { TMK_ERR_INDEX_FULL,          "index table full" },
    { TMK_ERR_CHECKPOINT_FULL,     "checkpoint table full" },
    { TMK_ERR_MIGRATION_FULL,      "migration table full" },
    { TMK_ERR_DEFERRED_FULL,       "deferred queue full" },
    { TMK_ERR_NOT_FOUND,           "object not found" },
    { TMK_ERR_BAD_STATE,           "invalid state" },
    { TMK_ERR_BAD_SEQUENCE,        "sequence mismatch" },
    { TMK_ERR_BAD_EPOCH,           "epoch mismatch" },
    { TMK_ERR_BAD_OWNER,           "token mismatch" },
    { TMK_ERR_BAD_OWNER,           "owner mismatch" },
    { TMK_ERR_NOT_ACTIVE,           "invalid lifecycle phase" },
    { TMK_ERR_BAD_FRAME_TYPE,      "unknown frame type" },
    { TMK_ERR_BAD_SUBREC_KIND,     "unknown subrecord kind" },
    { TMK_ERR_ALLOC,               "allocation failed" },
    { TMK_ERR_ALREADY_EXISTS,      "object already exists" },
    { TMK_ERR_NOT_ACTIVE,          "object not active" },
    { TMK_ERR_MIGRATION_ACTIVE,    "migration already active" },
    { TMK_ERR_OVERFLOW,            "overflow" },
    { TMK_ERR_UNDERFLOW,           "underflow" },
    { TMK_ERR_INVALID_ID,          "invalid identifier" },
    { TMK_ERR_BAD_STATE,          "dependency error" },
    { TMK_ERR_BAD_EPOCH, "generation mismatch" },
    { TMK_ERR_BAD_STATE,          "lease still held" },
    { TMK_ERR_ALREADY_EXISTS,       "cursor still active" },
    { TMK_ERR_BAD_STATE,        "compaction busy" },
    { TMK_ERR_RETENTION_SKIP,      "retention skipped" },
    { TMK_ERR_CHECKPOINT_STALE,    "checkpoint stale" },
};

const char *tmk_error_str(int err) {
    for (size_t i = 0; i < sizeof(error_table)/sizeof(error_table[0]); i++) {
        if (error_table[i].code == err) return error_table[i].msg;
    }
    return "unknown error";
}

tmk_severity_t tmk_error_severity(int error_code) {
    if (error_code >= 0) return TMK_SEVERITY_OK;

    switch (error_code) {
    case TMK_ERR_NOT_FOUND:
    case TMK_ERR_NOT_ACTIVE:
    case TMK_ERR_ALREADY_EXISTS:
    case TMK_ERR_RETENTION_SKIP:
        return TMK_SEVERITY_WARNING;

    case TMK_ERR_BAD_CRC:
    case TMK_ERR_BAD_FRAME_TYPE:
    case TMK_ERR_BAD_FRAME_LEN:
    case TMK_ERR_BAD_SUBREC_LEN:
    case TMK_ERR_BAD_SUBREC_KIND:
    case TMK_ERR_BAD_SEQUENCE:
    case TMK_ERR_BAD_EPOCH:
    case TMK_ERR_BAD_STATE:
    case TMK_ERR_BAD_OWNER:
    case TMK_ERR_BAD_VERSION:
    case TMK_ERR_INVALID_ID:
        return TMK_SEVERITY_ERROR;

    case TMK_ERR_ALLOC:
    case TMK_ERR_SHORT_BUF:
    case TMK_ERR_OVERFLOW:
    case TMK_ERR_UNDERFLOW:
        return TMK_SEVERITY_FATAL;

    default:
        return TMK_SEVERITY_ERROR;
    }
}

bool tmk_error_is_retriable(int error_code) {
    switch (error_code) {
    case TMK_ERR_NOT_FOUND:
    case TMK_ERR_NOT_ACTIVE:
    case TMK_ERR_RETENTION_SKIP:
    case TMK_ERR_CHECKPOINT_STALE:
        return true;
    default:
        return false;
    }
}

const char *tmk_severity_str(tmk_severity_t severity) {
    switch (severity) {
    case TMK_SEVERITY_OK:      return "OK";
    case TMK_SEVERITY_WARNING: return "WARNING";
    case TMK_SEVERITY_ERROR:   return "ERROR";
    case TMK_SEVERITY_FATAL:   return "FATAL";
    default:                   return "UNKNOWN";
    }
}
