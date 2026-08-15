# C++20 application and orchestration layer.

add_library(${HPCSIM_CPP_LIBRARY} INTERFACE)

target_link_libraries(${HPCSIM_CPP_LIBRARY} INTERFACE
    ${HPCSIM_C_LIBRARY})

target_compile_features(${HPCSIM_CPP_LIBRARY} INTERFACE cxx_std_20)
