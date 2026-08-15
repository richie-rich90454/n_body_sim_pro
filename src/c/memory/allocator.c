#include "n_body_sim_pro/memory/allocator.h"
#include "n_body_sim_pro/memory/allocation_tracker.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct NBodySimProAllocationHeader {
    size_t size;
    size_t alignment;
    NBodySimProAllocationCategory category;
    const char* source_file;
    int source_line;
} NBodySimProAllocationHeader;

static uintptr_t align_up(uintptr_t value, size_t alignment) {
    size_t remainder = (size_t)(value % (uintptr_t)alignment);
    if (remainder == 0) {
        return value;
    }
    return value + (uintptr_t)(alignment - remainder);
}

const char* n_body_sim_pro_allocation_category_string(NBodySimProAllocationCategory category) {
    switch (category) {
        case N_BODY_SIM_PRO_ALLOCATION_CATEGORY_PARTICLE_STORAGE:
            return "particle_storage";
        case N_BODY_SIM_PRO_ALLOCATION_CATEGORY_OCTREE_NODES:
            return "octree_nodes";
        case N_BODY_SIM_PRO_ALLOCATION_CATEGORY_THREAD_WORKSPACE:
            return "thread_workspace";
        case N_BODY_SIM_PRO_ALLOCATION_CATEGORY_TEMPORARY_BUFFER:
            return "temporary_buffer";
        case N_BODY_SIM_PRO_ALLOCATION_CATEGORY_CHECKPOINT:
            return "checkpoint";
        case N_BODY_SIM_PRO_ALLOCATION_CATEGORY_RENDERER:
            return "renderer";
        case N_BODY_SIM_PRO_ALLOCATION_CATEGORY_UI:
            return "ui";
        case N_BODY_SIM_PRO_ALLOCATION_CATEGORY_OTHER:
            return "other";
        case N_BODY_SIM_PRO_ALLOCATION_CATEGORY_COUNT:
            break;
    }
    return "unknown";
}

/*
 * Allocation layout:
 *
 *   [header][padding][aligned user memory ... size bytes]
 *   ^block                  ^ returned pointer
 *
 * The header lives at the start of the raw block, so on deallocation the
 * block base is recovered directly from the back-reference stored in the
 * bytes immediately before the aligned user pointer.
 */
void* n_body_sim_pro_allocate(size_t size, size_t alignment,
                      NBodySimProAllocationCategory category,
                      const char* source_file, int source_line) {
    if (size == 0) {
        return NULL;
    }
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    size_t header_size = sizeof(NBodySimProAllocationHeader);
    if (size > SIZE_MAX - alignment - header_size) {
        return NULL;
    }

    NBodySimProAllocationHeader* header =
        (NBodySimProAllocationHeader*)malloc(header_size + alignment + size);
    if (header == NULL) {
        return NULL;
    }

    uintptr_t aligned_address =
        align_up((uintptr_t)header + (uintptr_t)header_size, alignment);

    header->size = size;
    header->alignment = alignment;
    header->category = category;
    header->source_file = source_file;
    header->source_line = source_line;

    void** back_reference = (void**)(aligned_address - (uintptr_t)sizeof(void*));
    *back_reference = header;

    n_body_sim_pro_allocation_tracker_record(category, size, 1);

    return (void*)aligned_address;
}

void n_body_sim_pro_deallocate(void* pointer, const char* source_file, int source_line) {
    (void)source_file;
    (void)source_line;
    if (pointer == NULL) {
        return;
    }
    NBodySimProAllocationHeader** back_reference =
        (NBodySimProAllocationHeader**)((uintptr_t)pointer - (uintptr_t)sizeof(void*));
    NBodySimProAllocationHeader* header = *back_reference;
    n_body_sim_pro_allocation_tracker_record(header->category, header->size, 0);
    free(header);
}

int n_body_sim_pro_allocation_query(const void* pointer, size_t* size,
                            NBodySimProAllocationCategory* category) {
    if (pointer == NULL) {
        return 1;
    }
    NBodySimProAllocationHeader* const* back_reference =
        (NBodySimProAllocationHeader* const*)((uintptr_t)pointer - (uintptr_t)sizeof(void*));
    NBodySimProAllocationHeader* header = *back_reference;
    if (size != NULL) {
        *size = header->size;
    }
    if (category != NULL) {
        *category = header->category;
    }
    return 0;
}

void* n_body_sim_pro_reallocate(void* pointer, size_t new_size, const char* source_file,
                        int source_line) {
    if (pointer == NULL) {
        return n_body_sim_pro_allocate(new_size, sizeof(void*), N_BODY_SIM_PRO_ALLOCATION_CATEGORY_OTHER,
                               source_file, source_line);
    }
    if (new_size == 0) {
        return NULL;
    }
    NBodySimProAllocationHeader* const* back_reference =
        (NBodySimProAllocationHeader* const*)((uintptr_t)pointer - (uintptr_t)sizeof(void*));
    NBodySimProAllocationHeader* header = *back_reference;
    const size_t old_size = header->size;
    if (old_size >= new_size) {
        header->size = new_size;
        return pointer;
    }

    void* replacement = n_body_sim_pro_allocate(new_size, header->alignment, header->category,
                                        source_file, source_line);
    if (replacement == NULL) {
        return NULL;
    }
    memcpy(replacement, pointer, old_size);
    free(header);
    return replacement;
}
