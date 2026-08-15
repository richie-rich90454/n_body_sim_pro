#include "hpcsim/memory/allocator.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct HpcsimAllocationHeader {
    size_t size;
    size_t alignment;
    HpcsimAllocationCategory category;
    const char* source_file;
    int source_line;
} HpcsimAllocationHeader;

static uintptr_t align_up(uintptr_t value, size_t alignment) {
    size_t remainder = (size_t)(value % (uintptr_t)alignment);
    if (remainder == 0) {
        return value;
    }
    return value + (uintptr_t)(alignment - remainder);
}

const char* hpcsim_allocation_category_string(HpcsimAllocationCategory category) {
    switch (category) {
        case HPCSIM_ALLOCATION_CATEGORY_PARTICLE_STORAGE:
            return "particle_storage";
        case HPCSIM_ALLOCATION_CATEGORY_OCTREE_NODES:
            return "octree_nodes";
        case HPCSIM_ALLOCATION_CATEGORY_THREAD_WORKSPACE:
            return "thread_workspace";
        case HPCSIM_ALLOCATION_CATEGORY_TEMPORARY_BUFFER:
            return "temporary_buffer";
        case HPCSIM_ALLOCATION_CATEGORY_CHECKPOINT:
            return "checkpoint";
        case HPCSIM_ALLOCATION_CATEGORY_RENDERER:
            return "renderer";
        case HPCSIM_ALLOCATION_CATEGORY_UI:
            return "ui";
        case HPCSIM_ALLOCATION_CATEGORY_OTHER:
            return "other";
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
void* hpcsim_allocate(size_t size, size_t alignment,
                      HpcsimAllocationCategory category,
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

    size_t header_size = sizeof(HpcsimAllocationHeader);
    if (size > SIZE_MAX - alignment - header_size) {
        return NULL;
    }

    HpcsimAllocationHeader* header =
        (HpcsimAllocationHeader*)malloc(header_size + alignment + size);
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

    return (void*)aligned_address;
}

void hpcsim_deallocate(void* pointer, const char* source_file, int source_line) {
    (void)source_file;
    (void)source_line;
    if (pointer == NULL) {
        return;
    }
    HpcsimAllocationHeader** back_reference =
        (HpcsimAllocationHeader**)((uintptr_t)pointer - (uintptr_t)sizeof(void*));
    HpcsimAllocationHeader* header = *back_reference;
    free(header);
}

int hpcsim_allocation_query(const void* pointer, size_t* size,
                            HpcsimAllocationCategory* category) {
    if (pointer == NULL) {
        return 1;
    }
    HpcsimAllocationHeader* const* back_reference =
        (HpcsimAllocationHeader* const*)((uintptr_t)pointer - (uintptr_t)sizeof(void*));
    HpcsimAllocationHeader* header = *back_reference;
    if (size != NULL) {
        *size = header->size;
    }
    if (category != NULL) {
        *category = header->category;
    }
    return 0;
}

void* hpcsim_reallocate(void* pointer, size_t new_size, const char* source_file,
                        int source_line) {
    if (pointer == NULL) {
        return hpcsim_allocate(new_size, sizeof(void*), HPCSIM_ALLOCATION_CATEGORY_OTHER,
                               source_file, source_line);
    }
    if (new_size == 0) {
        return NULL;
    }
    HpcsimAllocationHeader* const* back_reference =
        (HpcsimAllocationHeader* const*)((uintptr_t)pointer - (uintptr_t)sizeof(void*));
    HpcsimAllocationHeader* header = *back_reference;
    const size_t old_size = header->size;
    if (old_size >= new_size) {
        header->size = new_size;
        return pointer;
    }

    void* replacement = hpcsim_allocate(new_size, header->alignment, header->category,
                                        source_file, source_line);
    if (replacement == NULL) {
        return NULL;
    }
    memcpy(replacement, pointer, old_size);
    free(header);
    return replacement;
}
