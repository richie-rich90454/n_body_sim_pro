#include "n_body_sim_pro/memory/allocator.h"
#include "test_harness.h"

#include <stdint.h>

static void test_aligned_allocation_sizes(void) {
    const size_t alignments[] = {1, 2, 4, 8, 16, 32, 64};
    for (size_t index = 0; index < sizeof(alignments) / sizeof(alignments[0]); ++index) {
        const size_t alignment = alignments[index];
        void* pointer = n_body_sim_pro_allocate(123, alignment,
                                        N_BODY_SIM_PRO_ALLOCATION_CATEGORY_TEMPORARY_BUFFER,
                                        __FILE__, __LINE__);
        N_BODY_SIM_PRO_ASSERT(pointer != NULL);
        if (pointer != NULL) {
            N_BODY_SIM_PRO_ASSERT(((uintptr_t)pointer % alignment) == 0);
            n_body_sim_pro_deallocate(pointer, __FILE__, __LINE__);
        }
    }
}

static void test_allocation_metadata_query(void) {
    void* pointer = n_body_sim_pro_allocate(4096, 64,
                                    N_BODY_SIM_PRO_ALLOCATION_CATEGORY_PARTICLE_STORAGE,
                                    __FILE__, __LINE__);
    N_BODY_SIM_PRO_ASSERT(pointer != NULL);
    if (pointer == NULL) {
        return;
    }
    size_t size = 0;
    NBodySimProAllocationCategory category = N_BODY_SIM_PRO_ALLOCATION_CATEGORY_OTHER;
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_allocation_query(pointer, &size, &category) == 0);
    N_BODY_SIM_PRO_ASSERT_EQ_SIZE(size, 4096);
    N_BODY_SIM_PRO_ASSERT(category == N_BODY_SIM_PRO_ALLOCATION_CATEGORY_PARTICLE_STORAGE);
    n_body_sim_pro_deallocate(pointer, __FILE__, __LINE__);
}

static void test_allocation_zero_size_rejected(void) {
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_allocate(0, 64, N_BODY_SIM_PRO_ALLOCATION_CATEGORY_OTHER,
                                  __FILE__, __LINE__) == NULL);
}

static void test_deallocate_null_is_safe(void) {
    n_body_sim_pro_deallocate(NULL, __FILE__, __LINE__);
    N_BODY_SIM_PRO_ASSERT(1);
}

static void test_category_names(void) {
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_allocation_category_string(
                      N_BODY_SIM_PRO_ALLOCATION_CATEGORY_PARTICLE_STORAGE) != NULL);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_allocation_category_string(
                      N_BODY_SIM_PRO_ALLOCATION_CATEGORY_OCTREE_NODES) != NULL);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_allocation_category_string(
                      N_BODY_SIM_PRO_ALLOCATION_CATEGORY_THREAD_WORKSPACE) != NULL);
}

int main(void) {
    N_BODY_SIM_PRO_TEST_SUITE_BEGIN();
    N_BODY_SIM_PRO_TEST_RUN(test_aligned_allocation_sizes);
    N_BODY_SIM_PRO_TEST_RUN(test_allocation_metadata_query);
    N_BODY_SIM_PRO_TEST_RUN(test_allocation_zero_size_rejected);
    N_BODY_SIM_PRO_TEST_RUN(test_deallocate_null_is_safe);
    N_BODY_SIM_PRO_TEST_RUN(test_category_names);
    return N_BODY_SIM_PRO_TEST_SUITE_END();
}
