/*
 * libtidemark — cursor-lease binding
 *
 * Binds a cursor to a lease context for read isolation.
 *
 * BUG SURFACE (lease_transfer_dangle): If migration revokes the
 * lease, the cursor's bound_lease pointer becomes dangling.
 */
#include "../internal/context.h"
#include "bind.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_cursor_bind_lease(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_LEASE_TOKEN)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint16_t cursor_id = frame->header.route_id;
    uint32_t lease_token = frame->subrecords[0].lease_token;

    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;

    /* Find lease by token */
    tmk_lease_t *lease = NULL;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].token == lease_token) {
            lease = &ctx->leases[i];
            break;
        }
    }
    if (!lease) return TMK_ERR_NOT_FOUND;

    cur->bound_lease = lease;
    return TMK_OK;
}
