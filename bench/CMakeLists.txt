add_executable(paragraph_cache_bench paragraph_cache/bench.cpp)
target_include_directories(paragraph_cache_bench PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(paragraph_cache_bench PRIVATE lambda)
set_target_properties(paragraph_cache_bench PROPERTIES CXX_STANDARD 23)
