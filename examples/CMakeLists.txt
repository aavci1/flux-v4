# Sample apps (link the static `flux` library). Included when FLUX_BUILD_EXAMPLES=ON at the project root (default OFF).

# Bundled fonts (Material Symbols, etc.) live next to each example binary so Application can register them at runtime.
function(flux_copy_example_resources target_name)
  add_custom_command(
    TARGET ${target_name}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../resources/fonts/MaterialSymbolsRounded.ttf"
            "$<TARGET_FILE_DIR:${target_name}>/fonts/MaterialSymbolsRounded.ttf"
    VERBATIM
    COMMENT "Copy bundled fonts for ${target_name}")
endfunction()

function(flux_add_example target_name)
  add_executable(${target_name} ${ARGN})
  target_link_libraries(${target_name} PRIVATE flux)
  if(FLUX_ENABLE_ASAN)
    target_compile_options(${target_name} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${target_name} PRIVATE -fsanitize=address)
  endif()
  flux_copy_example_resources(${target_name})
endfunction()

flux_add_example(animation-demo animation-demo/main.cpp)
flux_add_example(hello-world hello-world/main.cpp)
flux_add_example(lambda-studio lambda-studio/main.cpp)
flux_add_example(reactive-demo reactive-demo/main.cpp)
flux_add_example(scroll-demo scroll-demo/main.cpp)
flux_add_example(theme-demo theme-demo/main.cpp)
