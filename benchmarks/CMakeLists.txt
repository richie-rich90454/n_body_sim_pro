# Benchmark executables. Linked against the C engine only, headless.

add_executable(n_body_sim_pro_benchmark benchmark_force.c)
target_link_libraries(n_body_sim_pro_benchmark PRIVATE ${HPCSIM_C_LIBRARY})
if(NOT WIN32)
    target_link_libraries(n_body_sim_pro_benchmark PRIVATE m)
endif()
hpcsim_apply_warnings(n_body_sim_pro_benchmark)
