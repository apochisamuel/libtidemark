/*
 * libtidemark — error codes
 */
#ifndef TMK_ERROR_H
#define TMK_ERROR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TMK_OK                   0
#define TMK_ERR_SHORT_BUF       (-1)
#define TMK_ERR_BAD_MAGIC       (-2)
#define TMK_ERR_BAD_CRC         (-3)
#define TMK_ERR_BAD_VERSION     (-4)
#define TMK_ERR_BAD_FRAME_TYPE  (-5)
#define TMK_ERR_BAD_FRAME_LEN   (-6)
#define TMK_ERR_BAD_SUBREC_LEN  (-7)
#define TMK_ERR_BAD_SUBREC_KIND (-8)
#define TMK_ERR_BAD_STATE       (-9)
#define TMK_ERR_BAD_OWNER       (-10)
#define TMK_ERR_NOT_FOUND       (-11)
#define TMK_ERR_NOT_ACTIVE      (-12)
#define TMK_ERR_ALREADY_EXISTS  (-13)
#define TMK_ERR_ALLOC           (-14)
#define TMK_ERR_MIGRATION_ACTIVE (-15)
#define TMK_ERR_MIGRATION_FULL  (-16)
#define TMK_ERR_CHECKPOINT_FULL (-17)
#define TMK_ERR_CHECKPOINT_STALE (-18)
#define TMK_ERR_INVALID_ID      (-20)
#define TMK_ERR_OVERFLOW        (-21)
#define TMK_ERR_UNDERFLOW       (-22)
#define TMK_ERR_BAD_EPOCH       (-23)
#define TMK_ERR_BAD_SEQUENCE    (-24)
#define TMK_ERR_RETENTION_SKIP  (-25)
#define TMK_ERR_SESSION_FULL    (-30)
#define TMK_ERR_CHANNEL_FULL    (-31)
#define TMK_ERR_PARTITION_FULL  (-32)
#define TMK_ERR_SEGMENT_FULL    (-33)
#define TMK_ERR_CURSOR_FULL     (-34)
#define TMK_ERR_LEASE_FULL      (-35)
#define TMK_ERR_INDEX_FULL      (-36)
#define TMK_ERR_DEFERRED_FULL   (-37)

const char *tmk_error_str(int error_code);

typedef enum {
    TMK_SEVERITY_OK      = 0,
    TMK_SEVERITY_WARNING = 1,
    TMK_SEVERITY_ERROR   = 2,
    TMK_SEVERITY_FATAL   = 3
} tmk_severity_t;

tmk_severity_t tmk_error_severity(int error_code);
bool tmk_error_is_retriable(int error_code);
const char *tmk_severity_str(tmk_severity_t severity);

#ifdef __cplusplus
}
#endif

#endif
