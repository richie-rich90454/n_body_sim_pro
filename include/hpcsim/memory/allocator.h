#ifndef HPCSIM_MEMORY_ALLOCATOR_H
#define HPCSIM_MEMORY_ALLOCATOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Controlled allocation layer for HPCSim-internal buffers.
 *
 * This layer exists so the simulation engine can:
 *   - request aligned storage for SIMD-friendly SoA arrays
 *   - attribute every allocation to a category (particles, octree nodes,
 *     thread workspaces, ...) for developer instrumentation
 *   - keep allocation metadata available to the leak/allocation tracker
 *
 * Third-party libraries (SDL, OpenGL, Dear ImGui) never use this layer; the
 * engine never replaces the process-wide system allocator.
 */

typedef enum HpcsimAllocationCategory {
    HPCSIM_ALLOCATION_CATEGORY_PARTICLE_STORAGE,
    HPCSIM_ALLOCATION_CATEGORY_OCTREE_NODES,
    HPCSIM_ALLOCATION_CATEGORY_THREAD_WORKSPACE,
    HPCSIM_ALLOCATION_CATEGORY_TEMPORARY_BUFFER,
    HPCSIM_ALLOCATION_CATEGORY_CHECKPOINT,
    HPCSIM_ALLOCATION_CATEGORY_RENDERER,
    HPCSIM_ALLOCATION_CATEGORY_UI,
    HPCSIM_ALLOCATION_CATEGORY_OTHER,
    HPCSIM_ALLOCATION_CATEGORY_COUNT
} HpcsimAllocationCategory;

/* Human-readable name for an allocation category. Never returns NULL. */
const char* hpcsim_allocation_category_string(HpcsimAllocationCategory category);

/*
 * Allocate `size` bytes of memory aligned to `alignment` bytes.
 *
 * The returned pointer is `alignment`-aligned and suitably aligned for any
 * object of that size. Returns NULL when the request cannot be satisfied.
 * `source_file` and `source_line` attribute the allocation site for
 * instrumentation; they may be NULL/0.
 */
void* hpcsim_allocate(size_t size, size_t alignment,
                      HpcsimAllocationCategory category,
                      const char* source_file, int source_line);

/* Release a pointer previously returned by hpcsim_allocate. NULL-safe. */
void hpcsim_deallocate(void* pointer, const char* source_file, int source_line);

/*
 * Resize a pointer previously returned by hpcsim_allocate. The new size must
 * be non-zero; on success the returned pointer owns the data and the old
 * pointer must not be used. Returns NULL and leaves `pointer` valid on
 * failure. Alignment is preserved from the original allocation.
 */
void* hpcsim_reallocate(void* pointer, size_t new_size, const char* source_file,
                        int source_line);

/*
 * Retrieve metadata for a live allocation.
 *
 * Returns 0 when `pointer` is a valid hpcsim allocation, in which case the
 * out-parameters receive the stored metadata. Returns non-zero otherwise.
 * All out-parameters may be NULL.
 */
int hpcsim_allocation_query(const void* pointer, size_t* size,
                            HpcsimAllocationCategory* category);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_MEMORY_ALLOCATOR_H */
