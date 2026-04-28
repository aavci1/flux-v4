add_library(v5_reactive_prototype INTERFACE)
target_include_directories(v5_reactive_prototype INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(v5_reactive_prototype INTERFACE cxx_std_23)

set(FLUX_V5_PROTOTYPE_SANITIZER "" CACHE STRING
  "Sanitizer for v5 reactive prototype targets: ASAN, UBSAN, TSAN, or empty.")
set_property(CACHE FLUX_V5_PROTOTYPE_SANITIZER PROPERTY STRINGS "" ASAN UBSAN TSAN)

function(flux_v5_apply_prototype_flags target)
  if(FLUX_V5_PROTOTYPE_SANITIZER STREQUAL "ASAN")
    target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address)
  elseif(FLUX_V5_PROTOTYPE_SANITIZER STREQUAL "UBSAN")
    target_compile_options(${target} PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=undefined)
  elseif(FLUX_V5_PROTOTYPE_SANITIZER STREQUAL "TSAN")
    target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=thread)
  elseif(NOT FLUX_V5_PROTOTYPE_SANITIZER STREQUAL "")
    message(FATAL_ERROR
      "Unknown FLUX_V5_PROTOTYPE_SANITIZER=${FLUX_V5_PROTOTYPE_SANITIZER}")
  endif()
endfunction()

enable_testing()

function(flux_v5_add_prototype_test name source)
  add_executable(v5_proto_${name} ${source})
  target_link_libraries(v5_proto_${name} PRIVATE v5_reactive_prototype)
  flux_v5_apply_prototype_flags(v5_proto_${name})
  add_test(NAME v5_proto_${name} COMMAND v5_proto_${name})
  set_property(TEST v5_proto_${name} PROPERTY LABELS v5-prototype)
  set_property(GLOBAL APPEND PROPERTY FLUX_V5_PROTOTYPE_TEST_TARGETS v5_proto_${name})
endfunction()

function(flux_v5_add_prototype_benchmark name source)
  add_executable(v5_proto_${name} ${source})
  target_link_libraries(v5_proto_${name} PRIVATE v5_reactive_prototype)
  flux_v5_apply_prototype_flags(v5_proto_${name})
  set_property(GLOBAL APPEND PROPERTY FLUX_V5_PROTOTYPE_BENCH_TARGETS v5_proto_${name})
endfunction()

flux_v5_add_prototype_test(signal_tests tests/SignalTests.cpp)
flux_v5_add_prototype_test(computed_tests tests/ComputedTests.cpp)
flux_v5_add_prototype_test(effect_tests tests/EffectTests.cpp)
flux_v5_add_prototype_test(scope_tests tests/ScopeTests.cpp)
flux_v5_add_prototype_test(for_show_tests tests/ForShowTests.cpp)
flux_v5_add_prototype_test(diamond_tests tests/DiamondTests.cpp)

flux_v5_add_prototype_benchmark(effect_fire_bench benchmarks/EffectFireBench.cpp)
flux_v5_add_prototype_benchmark(memory_bench benchmarks/MemoryBench.cpp)
flux_v5_add_prototype_benchmark(ambient_loop_port benchmarks/AmbientLoopPort.cpp)

get_property(FLUX_V5_PROTOTYPE_TEST_TARGETS GLOBAL PROPERTY FLUX_V5_PROTOTYPE_TEST_TARGETS)
add_custom_target(v5_reactive_prototype_tests
  DEPENDS ${FLUX_V5_PROTOTYPE_TEST_TARGETS}
)

get_property(FLUX_V5_PROTOTYPE_BENCH_TARGETS GLOBAL PROPERTY FLUX_V5_PROTOTYPE_BENCH_TARGETS)
add_custom_target(v5_reactive_prototype_benchmarks
  DEPENDS ${FLUX_V5_PROTOTYPE_BENCH_TARGETS}
)
