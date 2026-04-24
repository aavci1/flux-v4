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

add_executable(alert-demo alert-demo/main.cpp)
target_link_libraries(alert-demo PRIVATE flux)

add_executable(animation-demo animation-demo/main.cpp)
target_link_libraries(animation-demo PRIVATE flux)

add_executable(blend-demo blend-demo/main.cpp)
target_link_libraries(blend-demo PRIVATE flux)

add_executable(button-demo button-demo/main.cpp)
target_link_libraries(button-demo PRIVATE flux)

add_executable(card-demo card-demo/main.cpp)
target_link_libraries(card-demo PRIVATE flux)

add_executable(checkbox-demo checkbox-demo/main.cpp)
target_link_libraries(checkbox-demo PRIVATE flux)

add_executable(cursor-demo cursor-demo/main.cpp)
target_link_libraries(cursor-demo PRIVATE flux)

add_executable(hello-world hello-world/main.cpp)
target_link_libraries(hello-world PRIVATE flux)

add_executable(icon-demo icon-demo/main.cpp)
target_link_libraries(icon-demo PRIVATE flux)

add_executable(image-demo image-demo/main.cpp)
target_link_libraries(image-demo PRIVATE flux)

set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/llama.cpp ${CMAKE_BINARY_DIR}/vendor/llama.cpp)
add_executable(lambda-studio lambda-studio/main.cpp lambda-studio/AppRuntime.cpp)
target_link_libraries(lambda-studio PRIVATE flux llama-common sqlite3)

add_executable(layout-demo layout-demo/main.cpp)
target_link_libraries(layout-demo PRIVATE flux)

add_executable(markdown-formatter-demo markdown-formatter-demo/main.cpp)
target_link_libraries(markdown-formatter-demo PRIVATE flux)

add_executable(popover-demo popover-demo/main.cpp)
target_link_libraries(popover-demo PRIVATE flux)

add_executable(reactive-demo reactive-demo/main.cpp)
target_link_libraries(reactive-demo PRIVATE flux)

add_executable(scene-graph-demo scene-graph-demo/main.cpp)
target_link_libraries(scene-graph-demo PRIVATE flux)

add_executable(scroll-demo scroll-demo/main.cpp)
target_link_libraries(scroll-demo PRIVATE flux)

add_executable(segmented-demo segmented-demo/main.cpp)
target_link_libraries(segmented-demo PRIVATE flux)

add_executable(select-demo select-demo/main.cpp)
target_link_libraries(select-demo PRIVATE flux)

add_executable(slider-demo slider-demo/main.cpp)
target_link_libraries(slider-demo PRIVATE flux)

add_executable(table-demo table-demo/main.cpp)
target_link_libraries(table-demo PRIVATE flux)

add_executable(text-demo text-demo/main.cpp)
target_link_libraries(text-demo PRIVATE flux)

add_executable(textinput-demo textinput-demo/main.cpp)
target_link_libraries(textinput-demo PRIVATE flux)

add_executable(theme-demo theme-demo/main.cpp)
target_link_libraries(theme-demo PRIVATE flux)

add_executable(toast-demo toast-demo/main.cpp)
target_link_libraries(toast-demo PRIVATE flux)

add_executable(toggle-demo toggle-demo/main.cpp)
target_link_libraries(toggle-demo PRIVATE flux)

add_executable(tooltip-demo tooltip-demo/main.cpp)
target_link_libraries(tooltip-demo PRIVATE flux)

add_executable(typography-demo typography-demo/main.cpp)
target_link_libraries(typography-demo PRIVATE flux)

set(FLUX_EXAMPLE_TARGETS
    alert-demo
    animation-demo
    blend-demo
    button-demo
    card-demo
    checkbox-demo
    cursor-demo
    hello-world
    icon-demo
    image-demo
    lambda-studio
    layout-demo
    markdown-formatter-demo
    popover-demo
    reactive-demo
    scene-graph-demo
    scroll-demo
    segmented-demo
    select-demo
    slider-demo
    table-demo
    text-demo
    textinput-demo
    theme-demo
    toast-demo
    toggle-demo
    tooltip-demo
    typography-demo
)
foreach(t ${FLUX_EXAMPLE_TARGETS})
  flux_copy_example_resources(${t})
endforeach()
