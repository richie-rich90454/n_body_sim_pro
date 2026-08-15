#ifndef N_BODY_SIM_PRO_MEMORY_ALLOCATION_TRACKER_H
#define N_BODY_SIM_PRO_MEMORY_ALLOCATION_TRACKER_H

#include "n_body_sim_pro/memory/allocator.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Allocation tracking for developer instrumentation.
 *
 * Tracker modes (spec):
 *   Normal         : disabled; near-zero overhead (one flag check per call).
 *   Developer      : enabled; allocation/free events are buffered per thread
 *                    and flushed in batches, so the hot path never does a
 *                    synchronous log or a per-allocation lock.
 *   Profiling      : maximum instrumentation; the shared event ring is
 *                    retained for leak and double-free detection.
 *
 * Events are recorded into a thread-local staging buffer by the allocator
 * and flushed into a shared ring under a single lock once the buffer fills.
 * This satisfies the "no malloc -> printf per allocation" rule and keeps
 * benchmark runs valid: when the tracker is disabled no events are recorded.
 */

enum {
    N_BODY_SIM_PRO_ALLOCATION_TRACKER_EVENT_RING_SIZE = 65536,
    N_BODY_SIM_PRO_ALLOCATION_TRACKER_THREAD_BUFFER_SIZE = 128
};

typedef struct NBodySimProAllocationEvent {
    uint64_t sequence;
    size_t size;
    NBodySimProAllocationCategory category;
    uint32_t thread_id;
    int is_allocation;
} NBodySimProAllocationEvent;

typedef struct NBodySimProAllocationSummary {
    size_t live_allocations;
    size_t total_allocations;
    size_t total_deallocations;
    size_t live_bytes;
    size_t peak_bytes;
    size_t total_allocated_bytes;
    size_t total_freed_bytes;
    double allocation_rate_per_second;
    double deallocation_rate_per_second;
    size_t dropped_events;
    size_t live_bytes_by_category[N_BODY_SIM_PRO_ALLOCATION_CATEGORY_COUNT];
    size_t live_allocations_by_category[N_BODY_SIM_PRO_ALLOCATION_CATEGORY_COUNT];
} NBodySimProAllocationSummary;

/* Enable (1) or disable (0) tracking. Disabling leaves counters intact. */
void n_body_sim_pro_allocation_tracker_set_enabled(int enabled);
int n_body_sim_pro_allocation_tracker_is_enabled(void);

/*
 * Record an allocation event (called by the allocator, not by users).
 * Thread-safe; buffers into thread-local storage and flushes in batches.
 */
void n_body_sim_pro_allocation_tracker_record(NBodySimProAllocationCategory category, size_t size,
                                      int is_allocation);

/*
 * Drain pending thread-local events and fill *summary with the current
 * totals and rates (rates computed over the window since the previous poll).
 * Returns 0 on success.
 */
int n_body_sim_pro_allocation_tracker_poll(NBodySimProAllocationSummary* summary);

/*
 * Report the last `count` (clamped) allocation events, newest first, for the
 * developer event stream. Returns the number of events written.
 */
size_t n_body_sim_pro_allocation_tracker_recent_events(NBodySimProAllocationEvent* events,
                                               size_t count);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_MEMORY_ALLOCATION_TRACKER_H */
