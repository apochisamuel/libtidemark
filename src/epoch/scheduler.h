#ifndef TMK_EPOCH_SCHEDULER_H
#define TMK_EPOCH_SCHEDULER_H

#include "../../include/tidemark/types.h"

/*
 * Epoch-driven event scheduler.
 * Registers callbacks for future epochs and dispatches them
 * when the epoch counter reaches the target.
 */

#define TMK_MAX_SCHEDULED 32

typedef enum {
    TMK_SCHED_RETENTION_EVAL = 0,
    TMK_SCHED_COMPACT_SWEEP  = 1,
    TMK_SCHED_CHECKPOINT     = 2,
    TMK_SCHED_DEFRAG         = 3,
    TMK_SCHED_REBALANCE      = 4,
    TMK_SCHED_LEASE_REVOKE   = 5,
    TMK_SCHED_CHANNEL_GC     = 6
} tmk_sched_event_type_t;

typedef struct {
    tmk_sched_event_type_t type;
    uint32_t               target_epoch;
    uint16_t               target_id;
    uint32_t               param;
    bool                   active;
    bool                   recurring;
    uint32_t               interval;
} tmk_scheduled_event_t;

typedef struct {
    tmk_scheduled_event_t events[TMK_MAX_SCHEDULED];
    uint16_t              count;
    uint32_t              last_dispatch_epoch;
    uint32_t              total_dispatched;
    uint32_t              total_missed;
} tmk_scheduler_t;

int  tmk_scheduler_init(tmk_scheduler_t *sched);
int  tmk_scheduler_add(tmk_scheduler_t *sched, tmk_sched_event_type_t type,
                       uint32_t target_epoch, uint16_t target_id,
                       uint32_t param, bool recurring, uint32_t interval);
int  tmk_scheduler_remove(tmk_scheduler_t *sched, int index);
int  tmk_scheduler_dispatch(tmk_scheduler_t *sched, tmk_context_t *ctx,
                            uint32_t current_epoch);
int  tmk_scheduler_pending_count(const tmk_scheduler_t *sched,
                                 uint32_t current_epoch);
void tmk_scheduler_clear(tmk_scheduler_t *sched);

#endif
