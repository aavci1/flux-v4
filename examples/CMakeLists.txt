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

add_executable(hello-world hello-world/main.cpp)
target_link_libraries(hello-world PRIVATE flux)
if(FLUX_ENABLE_ASAN)
  target_compile_options(hello-world PRIVATE -fsanitize=address -fno-omit-frame-pointer)
  target_link_options(hello-world PRIVATE -fsanitize=address)
endif()

set(FLUX_EXAMPLE_TARGETS
    hello-world
)
foreach(t ${FLUX_EXAMPLE_TARGETS})
  flux_copy_example_resources(${t})
endforeach()
