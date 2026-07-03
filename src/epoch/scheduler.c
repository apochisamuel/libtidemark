/*
 * libtidemark — epoch-driven event scheduler
 *
 * Provides deferred execution of maintenance operations at specific
 * future epochs.  Events can be one-shot or recurring (with a
 * configurable interval).
 *
 * Dispatch is called after each epoch advance.  Events whose
 * target_epoch <= current_epoch are executed.  Recurring events
 * are rescheduled by adding their interval to the target_epoch.
 *
 * Missed events (target_epoch < last_dispatch_epoch) are counted
 * but not retried — the system has already advanced past them.
 */
#include "scheduler.h"
#include "../retention/enforcer.h"
#include "../compact/engine.h"
#include "../index/defrag.h"
#include "../internal/context.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_scheduler_init(tmk_scheduler_t *sched) {
    if (!sched) return TMK_ERR_SHORT_BUF;
    memset(sched, 0, sizeof(*sched));
    return TMK_OK;
}

int tmk_scheduler_add(tmk_scheduler_t *sched, tmk_sched_event_type_t type,
                      uint32_t target_epoch, uint16_t target_id,
                      uint32_t param, bool recurring, uint32_t interval) {
    if (!sched) return TMK_ERR_SHORT_BUF;

    int slot = -1;
    for (int i = 0; i < TMK_MAX_SCHEDULED; i++) {
        if (!sched->events[i].active) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_OVERFLOW;

    sched->events[slot].type = type;
    sched->events[slot].target_epoch = target_epoch;
    sched->events[slot].target_id = target_id;
    sched->events[slot].param = param;
    sched->events[slot].active = true;
    sched->events[slot].recurring = recurring;
    sched->events[slot].interval = interval;
    sched->count++;

    return TMK_OK;
}

int tmk_scheduler_remove(tmk_scheduler_t *sched, int index) {
    if (!sched || index < 0 || index >= TMK_MAX_SCHEDULED)
        return TMK_ERR_INVALID_ID;
    if (!sched->events[index].active) return TMK_ERR_NOT_FOUND;

    sched->events[index].active = false;
    sched->count--;
    return TMK_OK;
}

static int dispatch_event(tmk_context_t *ctx, const tmk_scheduled_event_t *evt) {
    switch (evt->type) {
    case TMK_SCHED_RETENTION_EVAL:
        return tmk_retention_evaluate_watermarks(ctx);
    case TMK_SCHED_COMPACT_SWEEP: {
        /* Build a synthetic frame for compact_trigger */
        tmk_parsed_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        frame.header.frame_type = TMK_FRAME_COMPACT_TRIGGER;
        frame.header.session_id = ctx->active_session_id;
        frame.parsed_sub_count = 1;
        frame.subrecords[0].kind = TMK_SUB_CONTROL;
        return tmk_compact_trigger(ctx, &frame);
    }
    case TMK_SCHED_DEFRAG: {
        tmk_defrag_result_t result;
        return tmk_index_defrag(ctx, &result);
    }
    case TMK_SCHED_LEASE_REVOKE: {
        int revoked = 0;
        for (int i = 0; i < TMK_MAX_LEASES; i++) {
            if (ctx->leases[i].active &&
                ctx->leases[i].generation + 2 < ctx->global_generation) {
                ctx->leases[i].active = false;
                ctx->leases[i].phase = TMK_PHASE_FREED;
                revoked++;
            }
        }
        return revoked;
    }
    case TMK_SCHED_CHANNEL_GC: {
        int gc_count = 0;
        for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
            if (!ctx->channels[i].active) continue;
            uint32_t age = ctx->global_epoch - ctx->channels[i].create_epoch;
            if (age > ctx->channels[i].ttl_epochs * 2) {
                ctx->channels[i].active = false;
                ctx->channels[i].cached_segment_list = NULL;
                ctx->channels[i].cached_lease = NULL;
                ctx->channels[i].cached_epoch = NULL;
                gc_count++;
            }
        }
        return gc_count;
    }
    case TMK_SCHED_CHECKPOINT:
    case TMK_SCHED_REBALANCE:
        return TMK_OK;
    default:
        return TMK_ERR_BAD_STATE;
    }
}

int tmk_scheduler_dispatch(tmk_scheduler_t *sched, tmk_context_t *ctx,
                           uint32_t current_epoch) {
    if (!sched || !ctx) return TMK_ERR_SHORT_BUF;

    int dispatched = 0;

    for (int i = 0; i < TMK_MAX_SCHEDULED; i++) {
        if (!sched->events[i].active) continue;

        if (sched->events[i].target_epoch <= current_epoch) {
            /* Check if missed */
            if (sched->events[i].target_epoch < sched->last_dispatch_epoch) {
                sched->total_missed++;
            }

            dispatch_event(ctx, &sched->events[i]);
            dispatched++;
            sched->total_dispatched++;

            if (sched->events[i].recurring && sched->events[i].interval > 0) {
                sched->events[i].target_epoch = current_epoch +
                    sched->events[i].interval;
            } else {
                sched->events[i].active = false;
                sched->count--;
            }
        }
    }

    sched->last_dispatch_epoch = current_epoch;
    return dispatched;
}

int tmk_scheduler_pending_count(const tmk_scheduler_t *sched,
                                uint32_t current_epoch) {
    if (!sched) return 0;
    int pending = 0;
    for (int i = 0; i < TMK_MAX_SCHEDULED; i++) {
        if (sched->events[i].active &&
            sched->events[i].target_epoch <= current_epoch) {
            pending++;
        }
    }
    return pending;
}

void tmk_scheduler_clear(tmk_scheduler_t *sched) {
    if (!sched) return;
    memset(sched->events, 0, sizeof(sched->events));
    sched->count = 0;
}
