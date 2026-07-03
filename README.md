# libtidemark

A segmented time-series storage engine library for embedded and edge
deployments.

## Architecture

libtidemark organizes time-series data into **partitions** containing
**segments** (fixed-capacity data buffers). Data arrives through a
compact binary **wire protocol** with nested session headers, frames,
and subrecords. Each frame is CRC-32C validated.

### Subsystems

| Subsystem | Purpose |
|-----------|---------|
| wire      | Nested frame parser with CRC-32C validation |
| session   | Client connection lifecycle management |
| channel   | Multi-channel frame routing and dispatch |
| segment   | Data buffer allocation, sealing, retirement |
| partition | Partition-to-segment mappings, ownership |
| compact   | Segment merging, tombstone cleanup, deferred free |
| retention | Time-based retention policies, watermark tracking |
| lease     | Read-lease isolation with generation tracking |
| checkpoint| Snapshot manifest creation and restoration |
| migrate   | Cross-partition segment transfer and rebalancing |
| materialize| Query execution: cursor/index-based segment reads |
| cursor    | Iteration state, position tracking, caching |
| index     | Segment index nodes, key-range management |
| epoch     | Global epoch counter, generation boundaries |

## Building

```
make          # build static library
make asan     # build with AddressSanitizer
```

## Wire Protocol

Session header (24 bytes) → Frame[0..N] (32 byte header + payload)
→ Subrecord[0..M] (16 byte header + payload)

All integers are little-endian. CRC-32C (Castagnoli) validation
at session header and frame payload levels.

## License

MIT
