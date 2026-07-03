/*
 * libtidemark — migration commit
 *
 * Finalizes cross-partition migration by updating ownership tokens
 * and cleaning up the migration record.
 *
 * BUG SURFACE (migrated_partition_ref): Channel caches that hold
 * pointers to the source partition's segment list are NOT updated.
 * The commit changes partition state but does not notify channels.
 */
#include "../internal/context.h"
#include "commit.h"
#include "revoke.h"
#include "retire.h"
#include "../session/manager.h"
#include "../partition/registry.h"
#include "../../include/tidemark/error.h"

int tmk_migrate_commit(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_MIGRATION_DESC)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Find active migration */
    tmk_migration_t *mig = NULL;
    for (int i = 0; i < TMK_MAX_MIGRATIONS; i++) {
        if (ctx->migrations[i].active) { mig = &ctx->migrations[i]; break; }
    }
    if (!mig) return TMK_ERR_NOT_FOUND;

    /* Revoke leases for source partition */
    tmk_migrate_revoke_leases(ctx, mig->source_partition_id);

    /* Update destination partition generation */
    tmk_partition_t *dst = tmk_find_partition(ctx, mig->dest_partition_id);
    if (dst) {
        dst->generation = ctx->global_generation;
    }

    /* Finalize migration */
    mig->active = false;
    mig->phase = TMK_PHASE_RETIRED;

    return TMK_OK;
}
