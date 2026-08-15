#include "hpcsim/memory/allocator.h"
#include "test_harness.h"

#include <stdint.h>

static void test_aligned_allocation_sizes(void) {
    const size_t alignments[] = {1, 2, 4, 8, 16, 32, 64};
    for (size_t index = 0; index < sizeof(alignments) / sizeof(alignments[0]); ++index) {
        const size_t alignment = alignments[index];
        void* pointer = hpcsim_allocate(123, alignment,
                                        HPCSIM_ALLOCATION_CATEGORY_TEMPORARY_BUFFER,
                                        __FILE__, __LINE__);
        HPCSIM_ASSERT(pointer != NULL);
        if (pointer != NULL) {
            HPCSIM_ASSERT(((uintptr_t)pointer % alignment) == 0);
            hpcsim_deallocate(pointer, __FILE__, __LINE__);
        }
    }
}

static void test_allocation_metadata_query(void) {
    void* pointer = hpcsim_allocate(4096, 64,
                                    HPCSIM_ALLOCATION_CATEGORY_PARTICLE_STORAGE,
                                    __FILE__, __LINE__);
    HPCSIM_ASSERT(pointer != NULL);
    if (pointer == NULL) {
        return;
    }
    size_t size = 0;
    HpcsimAllocationCategory category = HPCSIM_ALLOCATION_CATEGORY_OTHER;
    HPCSIM_ASSERT(hpcsim_allocation_query(pointer, &size, &category) == 0);
    HPCSIM_ASSERT_EQ_SIZE(size, 4096);
    HPCSIM_ASSERT(category == HPCSIM_ALLOCATION_CATEGORY_PARTICLE_STORAGE);
    hpcsim_deallocate(pointer, __FILE__, __LINE__);
}

static void test_allocation_zero_size_rejected(void) {
    HPCSIM_ASSERT(hpcsim_allocate(0, 64, HPCSIM_ALLOCATION_CATEGORY_OTHER,
                                  __FILE__, __LINE__) == NULL);
}

static void test_deallocate_null_is_safe(void) {
    hpcsim_deallocate(NULL, __FILE__, __LINE__);
    HPCSIM_ASSERT(1);
}

static void test_category_names(void) {
    HPCSIM_ASSERT(hpcsim_allocation_category_string(
                      HPCSIM_ALLOCATION_CATEGORY_PARTICLE_STORAGE) != NULL);
    HPCSIM_ASSERT(hpcsim_allocation_category_string(
                      HPCSIM_ALLOCATION_CATEGORY_OCTREE_NODES) != NULL);
    HPCSIM_ASSERT(hpcsim_allocation_category_string(
                      HPCSIM_ALLOCATION_CATEGORY_THREAD_WORKSPACE) != NULL);
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_aligned_allocation_sizes);
    HPCSIM_TEST_RUN(test_allocation_metadata_query);
    HPCSIM_TEST_RUN(test_allocation_zero_size_rejected);
    HPCSIM_TEST_RUN(test_deallocate_null_is_safe);
    HPCSIM_TEST_RUN(test_category_names);
    return HPCSIM_TEST_SUITE_END();
}
