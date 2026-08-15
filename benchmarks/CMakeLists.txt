# Benchmark executables. Linked against the C engine only, headless.

add_executable(n_body_sim_pro_benchmark benchmark_force.c)
target_link_libraries(n_body_sim_pro_benchmark PRIVATE ${N_BODY_SIM_PRO_C_LIBRARY})
if(NOT WIN32)
    target_link_libraries(n_body_sim_pro_benchmark PRIVATE m)
endif()
n_body_sim_pro_apply_warnings(n_body_sim_pro_benchmark)
