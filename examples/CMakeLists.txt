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

add_executable(scene-graph scene-graph/main.cpp)
target_link_libraries(scene-graph PRIVATE flux)

add_executable(scene-graph-refresh scene-graph-refresh/main.cpp)
target_link_libraries(scene-graph-refresh PRIVATE flux)

add_executable(scene-graph-animation scene-graph-animation/main.cpp)
target_link_libraries(scene-graph-animation PRIVATE flux)

set(FLUX_EXAMPLE_TARGETS
    scene-graph
    scene-graph-refresh
    scene-graph-animation
)

foreach(t ${FLUX_EXAMPLE_TARGETS})
  flux_copy_example_resources(${t})
endforeach()
