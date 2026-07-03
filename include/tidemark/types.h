/*
 * libtidemark — segmented time-series storage engine
 * Public type definitions
 */
#ifndef TMK_TYPES_H
#define TMK_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Limits ─────────────────────────────────────────────── */
#define TMK_MAX_SESSIONS       16
#define TMK_MAX_CHANNELS       32
#define TMK_MAX_PARTITIONS     24
#define TMK_MAX_SEGMENTS       128
#define TMK_MAX_CURSORS        64
#define TMK_MAX_LEASES         64
#define TMK_MAX_INDEX_NODES    256
#define TMK_MAX_CHECKPOINTS    8
#define TMK_MAX_MIGRATIONS     4
#define TMK_MAX_DEFERRED       64
#define TMK_MAX_SUBRECORDS     16
#define TMK_MAX_SEGMENT_CAP    (64 * 1024)
#define TMK_MAX_FRAME_LEN      (128 * 1024)
#define TMK_RETENTION_EPOCH_THRESHOLD  3
#define TMK_COMPACT_SWEEP_THRESHOLD    2
#define TMK_CURSOR_COMPACT_THRESHOLD   6
#define TMK_GENERATION_BOUNDARY        4
#define TMK_TOMBSTONE_OCCUPANCY_THRESH 2
#define TMK_DEFERRED_DRAIN_THRESHOLD   3
#define TMK_CHANNEL_TTL_EPOCHS         4
#define TMK_REINDEX_WATERMARK          3
#define TMK_MERGE_OCCUPANCY_THRESHOLD  2

/* ── Wire constants ─────────────────────────────────────── */
#define TMK_MAGIC              0x544D4B31u
#define TMK_VERSION            0x0001u
#define TMK_SESSION_HDR_SIZE   24
#define TMK_FRAME_HDR_SIZE     32
#define TMK_SUBREC_HDR_SIZE    16

/* ── Frame types ────────────────────────────────────────── */
#define TMK_FRAME_SESSION_OPEN       0x0001
#define TMK_FRAME_SESSION_CLOSE      0x0002
#define TMK_FRAME_CHANNEL_REGISTER   0x0010
#define TMK_FRAME_CHANNEL_BIND       0x0011
#define TMK_FRAME_CHANNEL_CACHE_LEASE 0x0012
#define TMK_FRAME_PARTITION_CREATE   0x0020
#define TMK_FRAME_PARTITION_BIND     0x0021
#define TMK_FRAME_PARTITION_CACHE_EPOCH 0x0022
#define TMK_FRAME_SEGMENT_CREATE     0x0030
#define TMK_FRAME_SEGMENT_RETIRE     0x0031
#define TMK_FRAME_SEGMENT_META_INIT  0x0032
#define TMK_FRAME_DATA_INGEST        0x0040
#define TMK_FRAME_DATA_INGEST_IXWR   0x0041
#define TMK_FRAME_INDEX_REGISTER     0x0050
#define TMK_FRAME_INDEX_BUILD_CROSS  0x0051
#define TMK_FRAME_INDEX_BIND_CURSOR  0x0052
#define TMK_FRAME_INDEX_REG_CAPACITY 0x0053
#define TMK_FRAME_CURSOR_OPEN        0x0060
#define TMK_FRAME_CURSOR_CLOSE       0x0061
#define TMK_FRAME_CURSOR_CHECK       0x0065
#define TMK_FRAME_CURSOR_READ_BATCH  0x0066
#define TMK_FRAME_CURSOR_SEEK        0x0067
#define TMK_FRAME_CURSOR_ATTACH_IDX  0x0062
#define TMK_FRAME_CURSOR_BIND_LEASE  0x0063
#define TMK_FRAME_LEASE_ACQUIRE      0x0070
#define TMK_FRAME_LEASE_RELEASE      0x0071
#define TMK_FRAME_LEASE_BIND         0x0072
#define TMK_FRAME_LEASE_BIND_META    0x0073
#define TMK_FRAME_EPOCH_ADVANCE      0x0080
#define TMK_FRAME_RETENTION_TRIGGER  0x0090
#define TMK_FRAME_RETENTION_CONFIG   0x0091
#define TMK_FRAME_RETENTION_REINDEX  0x0092
#define TMK_FRAME_COMPACT_SCHEDULE   0x00A0
#define TMK_FRAME_COMPACT_TRIGGER    0x00A1
#define TMK_FRAME_COMPACT_MERGE      0x00A2
#define TMK_FRAME_PARTITION_VALIDATE  0x00B6
#define TMK_FRAME_COMPACT_MERGE_PART 0x00A3
#define TMK_FRAME_PARTITION_VALIDATE  0x00B6
#define TMK_FRAME_COMPACT_DEFER_DRAIN 0x00A4
#define TMK_FRAME_CHECKPOINT_CREATE  0x00B0
#define TMK_FRAME_CHECKPOINT_RESTORE 0x00B1
#define TMK_FRAME_MIGRATE_BEGIN      0x00C0
#define TMK_FRAME_MIGRATE_TRANSFER   0x00C1
#define TMK_FRAME_MIGRATE_COMMIT     0x00C2
#define TMK_FRAME_MIGRATE_ENUMERATE  0x00C3
#define TMK_FRAME_MATERIALIZE_EMIT   0x00D0
#define TMK_FRAME_MATERIALIZE_SCAN   0x00D1
#define TMK_FRAME_MATERIALIZE_RANGE  0x00D2
#define TMK_FRAME_MATERIALIZE_RESOLVE 0x00D3
#define TMK_FRAME_MATERIALIZE_READ   0x00D4
#define TMK_FRAME_MATERIALIZE_AGG_IDX 0x00A5
#define TMK_FRAME_MATERIALIZE_ISCAN  0x00D5

/* ── Subrecord kinds ────────────────────────────────────── */
#define TMK_SUB_DATA             0x0001
#define TMK_SUB_METADATA         0x0002
#define TMK_SUB_TOMBSTONE        0x0003
#define TMK_SUB_CONTROL          0x0004
#define TMK_SUB_INDEX_ENTRY      0x0005
#define TMK_SUB_LEASE_TOKEN      0x0006
#define TMK_SUB_CHECKPOINT_REF   0x0007
#define TMK_SUB_MIGRATION_DESC   0x0008
#define TMK_SUB_EPOCH_STATE      0x0009
#define TMK_SUB_RETENTION_POLICY 0x000A
#define TMK_SUB_CAPACITY_INFO    0x000B
#define TMK_SUB_CURSOR_POS       0x000C

/* ── Flags ──────────────────────────────────────────────── */
#define TMK_FLAG_URGENT          0x0001
#define TMK_FLAG_SEALED          0x0002
#define TMK_FLAG_TOMBSTONE       0x0004
#define TMK_FLAG_SNAPSHOT        0x0008
#define TMK_FLAG_MIGRATE         0x0010
#define TMK_FLAG_COMPACT         0x0020
#define TMK_FLAG_RETENTION       0x0040
#define TMK_FLAG_DEFERRED        0x0080

/* ── Lifecycle phases ───────────────────────────────────── */
typedef enum {
    TMK_PHASE_INIT = 0,
    TMK_PHASE_ACTIVE,
    TMK_PHASE_SEALED,
    TMK_PHASE_RETIRING,
    TMK_PHASE_RETIRED,
    TMK_PHASE_FREED
} tmk_phase_t;

/* ── Parsed wire structures ─────────────────────────────── */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint32_t session_id;
    uint32_t epoch;
    uint16_t frame_count;
    uint16_t reserved;
    uint32_t header_crc;
} tmk_session_header_t;

typedef struct {
    uint32_t length;
    uint16_t frame_type;
    uint32_t sequence;
    uint32_t session_id;
    uint16_t channel_id;
    uint16_t partition_id;
    uint16_t route_id;
    uint32_t owner_token;
    uint16_t flags;
    uint16_t subrecord_count;
    uint32_t payload_crc;
} tmk_frame_header_t;

typedef struct {
    uint16_t kind;
    uint16_t length;
    uint32_t local_epoch;
    uint32_t generation;
    uint32_t lease_token;
    const uint8_t *payload;
} tmk_subrecord_t;

typedef struct {
    tmk_frame_header_t header;
    tmk_subrecord_t    subrecords[TMK_MAX_SUBRECORDS];
    uint16_t           parsed_sub_count;
} tmk_parsed_frame_t;

/* ── Forward declarations ───────────────────────────────── */
typedef struct tmk_context    tmk_context_t;
typedef struct tmk_session    tmk_session_t;
typedef struct tmk_channel    tmk_channel_t;
typedef struct tmk_partition  tmk_partition_t;
typedef struct tmk_segment    tmk_segment_t;
typedef struct tmk_cursor     tmk_cursor_t;
typedef struct tmk_lease      tmk_lease_t;
typedef struct tmk_index_node tmk_index_node_t;
typedef struct tmk_checkpoint tmk_checkpoint_t;
typedef struct tmk_migration  tmk_migration_t;
typedef struct tmk_epoch_state tmk_epoch_state_t;

/* ── Segment metadata ───────────────────────────────────── */
typedef struct {
    uint32_t   checksum;
    uint32_t   min_ts;
    uint32_t   max_ts;
    uint16_t   record_count;
    uint16_t   index_depth;
    uint8_t    summary[64];
} tmk_segment_meta_t;

/* ── Segment ────────────────────────────────────────────── */
struct tmk_segment {
    uint16_t       id;
    uint16_t       partition_id;
    tmk_phase_t    phase;
    uint8_t       *data;
    size_t         capacity;
    size_t         used;
    uint32_t       generation;
    uint32_t       create_epoch;
    uint32_t       owner_token;
    uint32_t       compact_refcount;
    uint32_t       lease_refcount;
    bool           tombstone;
    bool           occupied;
    tmk_segment_meta_t *meta;
};

struct tmk_session {
    uint32_t       id;
    bool           active;
    uint32_t       epoch;
    uint32_t       sequence;
    uint32_t       lease_count;
    tmk_phase_t    phase;
};

struct tmk_epoch_state {
    uint32_t       global_epoch;
    uint32_t       generation;
    uint32_t       session_id;
    uint32_t       advance_count;
    bool           active;
};

struct tmk_channel {
    uint16_t       id;
    bool           active;
    uint32_t       session_id;
    uint16_t       bound_partition_id;
    tmk_segment_t *cached_segment_list;
    tmk_lease_t   *cached_lease;
    tmk_epoch_state_t *cached_epoch;
    uint8_t           *cached_channel_desc; /* cached from bound channel */
    uint32_t       create_epoch;
    uint32_t       ttl_epochs;
    uint32_t       generation;
    uint8_t       *channel_data;  /* heap-allocated descriptor */
    size_t         channel_data_len;
};

struct tmk_partition {
    uint16_t       id;
    bool           active;
    uint32_t       owner_token;
    uint16_t       segment_ids[TMK_MAX_SEGMENTS];
    uint16_t       segment_count;
    uint16_t       channel_ids[TMK_MAX_CHANNELS];
    uint16_t       channel_count;
    tmk_epoch_state_t *cached_epoch;
    uint8_t           *cached_channel_desc; /* cached from bound channel */
    uint32_t       generation;
    uint32_t       create_epoch;
    uint32_t       occupancy;
    tmk_phase_t    phase;
};

struct tmk_cursor {
    uint16_t       id;
    bool           active;
    uint32_t       session_id;
    uint16_t       segment_id;
    uint8_t       *cached_data;
    size_t         cached_capacity;
    size_t         position;
    uint32_t       generation;
    uint32_t       create_epoch;
    tmk_lease_t   *bound_lease;
    tmk_index_node_t *attached_index;
    uint16_t       partition_id;
};

struct tmk_lease {
    uint16_t       id;
    bool           active;
    uint32_t       session_id;
    uint16_t       segment_id;
    uint32_t       generation;
    uint32_t       token;
    uint32_t       create_epoch;
    tmk_phase_t    phase;
    uint8_t       *bound_data;
    size_t         bound_capacity;
    void          *bound_metadata;
};

struct tmk_index_node {
    uint16_t       id;
    bool           active;
    uint16_t       segment_id;
    uint16_t       partition_id;
    uint32_t       generation;
    uint32_t       stored_capacity;
    tmk_segment_t *segment_ref;
    tmk_partition_t *partition_ref;
    tmk_cursor_t  *cursor_ref;
    bool           tombstone;
    uint32_t       create_epoch;
    uint32_t       key_min;
    uint32_t       key_max;
    uint16_t       depth;
    uint16_t       child_count;
};

struct tmk_checkpoint {
    uint16_t       id;
    bool           active;
    uint32_t       epoch;
    uint16_t       segment_ids[TMK_MAX_SEGMENTS];
    uint16_t       segment_count;
    tmk_segment_t *segment_refs[TMK_MAX_SEGMENTS];
    uint8_t       *data_snapshots[TMK_MAX_SEGMENTS];  /* captured data pointers */
    size_t         snapshot_sizes[TMK_MAX_SEGMENTS];
    uint32_t       generation;
};

struct tmk_migration {
    uint16_t       id;
    bool           active;
    uint16_t       source_partition_id;
    uint16_t       dest_partition_id;
    uint32_t       owner_token;
    uint32_t       generation;
    tmk_phase_t    phase;
    uint16_t       transferred_segments[TMK_MAX_SEGMENTS];
    uint16_t       transferred_count;
};

typedef struct {
    uint16_t       segment_id;
    uint32_t       enqueue_epoch;
    uint32_t       compact_refcount;
    bool           occupied;
} tmk_deferred_entry_t;

typedef struct {
    uint32_t       watermark_epoch;
    uint32_t       ttl_epochs;
    bool           cascade_enabled;
    bool           configured;
} tmk_retention_policy_t;

struct tmk_context {
    tmk_session_t      sessions[TMK_MAX_SESSIONS];
    tmk_channel_t      channels[TMK_MAX_CHANNELS];
    tmk_partition_t    partitions[TMK_MAX_PARTITIONS];
    tmk_segment_t      segments[TMK_MAX_SEGMENTS];
    tmk_cursor_t       cursors[TMK_MAX_CURSORS];
    tmk_lease_t        leases[TMK_MAX_LEASES];
    tmk_index_node_t   index_nodes[TMK_MAX_INDEX_NODES];
    tmk_checkpoint_t   checkpoints[TMK_MAX_CHECKPOINTS];
    tmk_migration_t    migrations[TMK_MAX_MIGRATIONS];
    tmk_deferred_entry_t deferred[TMK_MAX_DEFERRED];
    tmk_epoch_state_t  epoch_states[TMK_MAX_SESSIONS];
    tmk_epoch_state_t *epoch_heap[TMK_MAX_SESSIONS]; /* heap-allocated copies */
    tmk_retention_policy_t retention;

    uint32_t           global_epoch;
    uint32_t           global_generation;
    uint32_t           global_sequence;
    uint32_t           active_session_id;
    uint16_t           deferred_count;
    uint16_t           tombstone_count;
    uint16_t           cursor_churn_count;

    bool               session_header_valid;
    tmk_session_header_t current_header;
};

#ifdef __cplusplus
}
#endif

#endif /* TMK_TYPES_H */
