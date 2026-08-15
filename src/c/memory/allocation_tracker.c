#include "hpcsim/memory/allocation_tracker.h"

#include <stdatomic.h>
#include <string.h>
#include <time.h>

/*
 * Thread-local event staging.
 *
 * The allocator calls hpcsim_allocation_tracker_record on every tracked
 * allocation/free. Events land in a thread-local buffer (no locking), and
 * when the buffer fills it is flushed into the shared ring under one mutex.
 * This keeps per-allocation cost to a TLS store plus one branch.
 */

static __thread HpcsimAllocationEvent thread_events[HPCSIM_ALLOCATION_TRACKER_THREAD_BUFFER_SIZE];
static __thread unsigned int thread_event_count = 0;
static __thread int thread_event_staged = 0;

static atomic_int tracker_enabled;
static atomic_uint_fast64_t next_sequence;

/*
 * The shared tracker state is protected by a spinlock. Contention is low:
 * a thread acquires it at most once per HPCSIM_ALLOCATION_TRACKER_THREAD_BUFFER_SIZE
 * events, so the tracking hot path stays lock-free.
 */
static atomic_flag tracker_mutex = ATOMIC_FLAG_INIT;

static void tracker_lock(void) {
    while (atomic_flag_test_and_set_explicit(&tracker_mutex, memory_order_acquire)) {
        /* spin */
    }
}

static void tracker_unlock(void) {
    atomic_flag_clear_explicit(&tracker_mutex, memory_order_release);
}

typedef struct TrackerState {
    size_t live_allocations;
    size_t total_allocations;
    size_t total_deallocations;
    size_t live_bytes;
    size_t peak_bytes;
    size_t total_allocated_bytes;
    size_t total_freed_bytes;
    size_t dropped_events;
    size_t live_bytes_by_category[HPCSIM_ALLOCATION_CATEGORY_COUNT];
    size_t live_allocations_by_category[HPCSIM_ALLOCATION_CATEGORY_COUNT];
    size_t allocations_since_poll;
    size_t deallocations_since_poll;
    double poll_window_seconds;
    HpcsimAllocationEvent ring[HPCSIM_ALLOCATION_TRACKER_EVENT_RING_SIZE];
    size_t ring_count;
    size_t ring_next;
} TrackerState;

static TrackerState tracker_state;
static int state_initialized = 0;

static double wall_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1.0e-9;
}

static void initialize_state(void) {
    if (!state_initialized) {
        memset(&tracker_state, 0, sizeof(tracker_state));
        tracker_state.poll_window_seconds = 1.0;
        state_initialized = 1;
    }
}

void hpcsim_allocation_tracker_set_enabled(int enabled) {
    initialize_state();
    atomic_store(&tracker_enabled, enabled ? 1 : 0);
}

int hpcsim_allocation_tracker_is_enabled(void) {
    return atomic_load(&tracker_enabled);
}

static unsigned int current_thread_id(void) {
    /* Pointer hash of a thread-local marker gives a stable per-thread id. */
    return (unsigned int)(((uintptr_t)&thread_event_staged >> 4) & 0xFFFFu);
}

static void flush_thread_buffer(void) {
    if (thread_event_count == 0) {
        return;
    }
    TrackerState* state = &tracker_state;
    tracker_lock();
    for (unsigned int i = 0; i < thread_event_count; ++i) {
        const HpcsimAllocationEvent* event = &thread_events[i];
        if (event->is_allocation) {
            ++state->live_allocations;
            ++state->total_allocations;
            ++state->allocations_since_poll;
            state->live_bytes += event->size;
            state->total_allocated_bytes += event->size;
            state->live_allocations_by_category[event->category]++;
            state->live_bytes_by_category[event->category] += event->size;
            if (state->live_bytes > state->peak_bytes) {
                state->peak_bytes = state->live_bytes;
            }
        } else {
            if (state->live_allocations > 0) {
                --state->live_allocations;
            }
            ++state->total_deallocations;
            ++state->deallocations_since_poll;
            if (event->size <= state->live_bytes) {
                state->live_bytes -= event->size;
            } else {
                state->live_bytes = 0;
            }
            state->total_freed_bytes += event->size;
            if (state->live_allocations_by_category[event->category] > 0) {
                --state->live_allocations_by_category[event->category];
            }
            if (event->size <= state->live_bytes_by_category[event->category]) {
                state->live_bytes_by_category[event->category] -= event->size;
            } else {
                state->live_bytes_by_category[event->category] = 0;
            }
        }
        state->ring[state->ring_next] = *event;
        state->ring_next = (state->ring_next + 1) % HPCSIM_ALLOCATION_TRACKER_EVENT_RING_SIZE;
        if (state->ring_count < HPCSIM_ALLOCATION_TRACKER_EVENT_RING_SIZE) {
            ++state->ring_count;
        }
    }
    tracker_unlock();
    thread_event_count = 0;
}

void hpcsim_allocation_tracker_record(HpcsimAllocationCategory category, size_t size,
                                      int is_allocation) {
    if (!atomic_load(&tracker_enabled)) {
        return;
    }
    if (thread_event_count >= HPCSIM_ALLOCATION_TRACKER_THREAD_BUFFER_SIZE) {
        flush_thread_buffer();
    }
    HpcsimAllocationEvent* event = &thread_events[thread_event_count++];
    event->sequence = atomic_fetch_add(&next_sequence, 1);
    event->size = size;
    event->category = category;
    event->thread_id = current_thread_id();
    event->is_allocation = is_allocation;
}

int hpcsim_allocation_tracker_poll(HpcsimAllocationSummary* summary) {
    if (summary == NULL) {
        return 1;
    }
    initialize_state();
    flush_thread_buffer();

    TrackerState* state = &tracker_state;
    const double now = wall_time_seconds();
    const double window = now - state->poll_window_seconds;
    if (window > 0.0) {
        summary->allocation_rate_per_second =
            (double)state->allocations_since_poll / window;
        summary->deallocation_rate_per_second =
            (double)state->deallocations_since_poll / window;
    } else {
        summary->allocation_rate_per_second = 0.0;
        summary->deallocation_rate_per_second = 0.0;
    }
    state->allocations_since_poll = 0;
    state->deallocations_since_poll = 0;
    state->poll_window_seconds = now;

    summary->live_allocations = state->live_allocations;
    summary->total_allocations = state->total_allocations;
    summary->total_deallocations = state->total_deallocations;
    summary->live_bytes = state->live_bytes;
    summary->peak_bytes = state->peak_bytes;
    summary->total_allocated_bytes = state->total_allocated_bytes;
    summary->total_freed_bytes = state->total_freed_bytes;
    summary->dropped_events = state->dropped_events;
    memcpy(summary->live_bytes_by_category, state->live_bytes_by_category,
           sizeof(state->live_bytes_by_category));
    memcpy(summary->live_allocations_by_category, state->live_allocations_by_category,
           sizeof(state->live_allocations_by_category));
    return 0;
}

size_t hpcsim_allocation_tracker_recent_events(HpcsimAllocationEvent* events,
                                               size_t count) {
    initialize_state();
    flush_thread_buffer();
    TrackerState* state = &tracker_state;
    if (count == 0) {
        return 0;
    }
    const size_t available =
        state->ring_count < HPCSIM_ALLOCATION_TRACKER_EVENT_RING_SIZE
            ? state->ring_count
            : HPCSIM_ALLOCATION_TRACKER_EVENT_RING_SIZE;
    const size_t to_write = available < count ? available : count;
    const size_t first = (state->ring_next + HPCSIM_ALLOCATION_TRACKER_EVENT_RING_SIZE -
                          to_write) %
                         HPCSIM_ALLOCATION_TRACKER_EVENT_RING_SIZE;
    for (size_t i = 0; i < to_write; ++i) {
        events[i] =
            state->ring[(first + i) % HPCSIM_ALLOCATION_TRACKER_EVENT_RING_SIZE];
    }
    return to_write;
}
