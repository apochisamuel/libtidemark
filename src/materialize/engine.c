/*
 * libtidemark — materialization engine
 *
 * Dispatches materialization requests to the appropriate read path
 * (emit, scan, range, resolve, iscan, read).
 */
#include "engine.h"
#include "emit.h"
#include "scan.h"
#include "range.h"
#include "resolve.h"
#include "iscan.h"
#include "read.h"
#include "../../include/tidemark/error.h"

int tmk_materialize_dispatch(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    switch (frame->header.frame_type) {
    case TMK_FRAME_MATERIALIZE_EMIT:
        return tmk_materialize_emit(ctx, frame);
    case TMK_FRAME_MATERIALIZE_SCAN:
        return tmk_materialize_scan_partition(ctx, frame);
    case TMK_FRAME_MATERIALIZE_RANGE:
        return tmk_materialize_read_range(ctx, frame);
    case TMK_FRAME_MATERIALIZE_RESOLVE:
        return tmk_materialize_index_resolve(ctx, frame);
    case TMK_FRAME_MATERIALIZE_ISCAN:
        return tmk_materialize_index_scan(ctx, frame);
    case TMK_FRAME_MATERIALIZE_READ:
        return tmk_materialize_read_lease(ctx, frame);
    default:
        return TMK_ERR_BAD_FRAME_TYPE;
    }
}
