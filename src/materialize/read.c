/*
 * libtidemark — lease-validated materialization
 *
 * BUG SURFACE (lease_transfer_dangle): Cursor's bound_lease
 * may be freed by migration.
 */
#include "../internal/context.h"
#include "read.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_materialize_read_lease(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    uint16_t cursor_id = frame->header.route_id;
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;

    /* Validate lease */
    if (cur->bound_lease) {
        if (!cur->bound_lease->active) return TMK_ERR_NOT_ACTIVE;

        if (cur->bound_lease->bound_data) {
            volatile uint8_t check = cur->bound_lease->bound_data[0];
            (void)check;
        }
    }

    /* Read from cached data */
    if (cur->cached_data && cur->position < cur->cached_capacity) {
        volatile uint8_t data = cur->cached_data[cur->position];
        (void)data;
        cur->position++;
    }

    return TMK_OK;
}
