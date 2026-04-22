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

add_executable(hello_world hello-world/main.cpp)
target_link_libraries(hello_world PRIVATE flux)

set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/llama.cpp
                 ${CMAKE_BINARY_DIR}/vendor/llama.cpp)

add_executable(lambda-studio
  lambda-studio/main.cpp
  lambda-studio/AppRuntime.cpp
)
target_link_libraries(lambda-studio PRIVATE flux llama-common sqlite3)

add_executable(clock_demo clock-demo/main.cpp)
target_link_libraries(clock_demo PRIVATE flux)

add_executable(blend_demo blend-demo/main.cpp)
target_link_libraries(blend_demo PRIVATE flux)

add_executable(text_demo text-demo/main.cpp)
target_link_libraries(text_demo PRIVATE flux)

add_executable(typography_demo typography-demo/main.cpp)
target_link_libraries(typography_demo PRIVATE flux)

add_executable(theme_demo theme-demo/main.cpp)
target_link_libraries(theme_demo PRIVATE flux)

add_executable(image_demo image-demo/main.cpp)
target_link_libraries(image_demo PRIVATE flux)

add_executable(reactive_demo reactive-demo/main.cpp)
target_link_libraries(reactive_demo PRIVATE flux)

add_executable(animation_demo animation-demo/main.cpp)
target_link_libraries(animation_demo PRIVATE flux)

add_executable(card_demo card-demo/main.cpp)
target_link_libraries(card_demo PRIVATE flux)

add_executable(table_demo table-demo/main.cpp)
target_link_libraries(table_demo PRIVATE flux)

add_executable(toast_demo toast-demo/main.cpp)
target_link_libraries(toast_demo PRIVATE flux)

add_executable(scroll_demo scroll-demo/main.cpp)
target_link_libraries(scroll_demo PRIVATE flux)

add_executable(cursor_demo cursor-demo/main.cpp)
target_link_libraries(cursor_demo PRIVATE flux)

add_executable(alert_demo alert-demo/main.cpp)
target_link_libraries(alert_demo PRIVATE flux)

add_executable(textinput_demo textinput-demo/main.cpp)
target_link_libraries(textinput_demo PRIVATE flux)

add_executable(markdown_styler_demo markdown-styler-demo/main.cpp)
target_link_libraries(markdown_styler_demo PRIVATE flux)

add_executable(button_demo button-demo/main.cpp)
target_link_libraries(button_demo PRIVATE flux)

add_executable(toggle_demo toggle-demo/main.cpp)
target_link_libraries(toggle_demo PRIVATE flux)

add_executable(checkbox_demo checkbox-demo/main.cpp)
target_link_libraries(checkbox_demo PRIVATE flux)

add_executable(slider_demo slider-demo/main.cpp)
target_link_libraries(slider_demo PRIVATE flux)

add_executable(select_demo select-demo/main.cpp)
target_link_libraries(select_demo PRIVATE flux)

add_executable(segmented_demo segmented-demo/main.cpp)
target_link_libraries(segmented_demo PRIVATE flux)

add_executable(popover_demo popover-demo/main.cpp)
target_link_libraries(popover_demo PRIVATE flux)

add_executable(tooltip_demo tooltip-demo/main.cpp)
target_link_libraries(tooltip_demo PRIVATE flux)

add_executable(icon_demo icon-demo/main.cpp)
target_link_libraries(icon_demo PRIVATE flux)

add_executable(layout_demo layout-demo/main.cpp)
target_link_libraries(layout_demo PRIVATE flux)

set(FLUX_EXAMPLE_TARGETS
    hello_world
    lambda-studio
    clock_demo
    blend_demo
    text_demo
    typography_demo
    theme_demo
    image_demo
    reactive_demo
    animation_demo
    card_demo
    table_demo
    toast_demo
    scroll_demo
    cursor_demo
    alert_demo
    textinput_demo
    markdown_styler_demo
    button_demo
    toggle_demo
    checkbox_demo
    slider_demo
    select_demo
    segmented_demo
    popover_demo
    tooltip_demo
    icon_demo
    layout_demo
)
foreach(t ${FLUX_EXAMPLE_TARGETS})
  flux_copy_example_resources(${t})
endforeach()
