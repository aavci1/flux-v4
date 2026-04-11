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

add_executable(clock_demo clock-demo/main.cpp)
target_link_libraries(clock_demo PRIVATE flux)

add_executable(blend_demo blend-demo/main.cpp)
target_link_libraries(blend_demo PRIVATE flux)

add_executable(text_demo text-demo/main.cpp)
target_link_libraries(text_demo PRIVATE flux)

add_executable(typography_demo typography-demo/main.cpp)
target_link_libraries(typography_demo PRIVATE flux)

add_executable(image_demo image-demo/main.cpp)
target_link_libraries(image_demo PRIVATE flux)

add_executable(scene_demo scene-demo/main.cpp)
target_link_libraries(scene_demo PRIVATE flux)

add_executable(reactive_demo reactive-demo/main.cpp)
target_link_libraries(reactive_demo PRIVATE flux)

add_executable(card_demo card-demo/main.cpp)
target_link_libraries(card_demo PRIVATE flux)

add_executable(scroll_demo scroll-demo/main.cpp)
target_link_libraries(scroll_demo PRIVATE flux)

add_executable(focus_demo focus-demo/main.cpp)
target_link_libraries(focus_demo PRIVATE flux)

add_executable(request_focus_demo request-focus-demo/main.cpp)
target_link_libraries(request_focus_demo PRIVATE flux)

add_executable(memo_demo memo-demo/main.cpp)
target_link_libraries(memo_demo PRIVATE flux)

add_executable(cursor_demo cursor-demo/main.cpp)
target_link_libraries(cursor_demo PRIVATE flux)

add_executable(foreach_demo foreach-demo/main.cpp)
target_link_libraries(foreach_demo PRIVATE flux)

add_executable(hover_demo hover-demo/main.cpp)
target_link_libraries(hover_demo PRIVATE flux)

add_executable(overlay_confirm_demo overlay-confirm-demo/main.cpp)
target_link_libraries(overlay_confirm_demo PRIVATE flux)

add_executable(alert_demo alert-demo/main.cpp)
target_link_libraries(alert_demo PRIVATE flux)

add_executable(actions_editor_demo actions-editor-demo/main.cpp)
target_link_libraries(actions_editor_demo PRIVATE flux)

add_executable(textinput_demo textinput-demo/main.cpp)
target_link_libraries(textinput_demo PRIVATE flux)

add_executable(textarea_demo textarea-demo/main.cpp)
target_link_libraries(textarea_demo PRIVATE flux)

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

add_executable(popover_demo popover-demo/main.cpp)
target_link_libraries(popover_demo PRIVATE flux)

add_executable(tooltip_demo tooltip-demo/main.cpp)
target_link_libraries(tooltip_demo PRIVATE flux)

add_executable(picker_demo picker-demo/main.cpp)
target_link_libraries(picker_demo PRIVATE flux)

add_executable(icon_demo icon-demo/main.cpp)
target_link_libraries(icon_demo PRIVATE flux)

add_executable(theme_demo theme-demo/main.cpp)
target_link_libraries(theme_demo PRIVATE flux)

add_executable(layout_demo layout-demo/main.cpp)
target_link_libraries(layout_demo PRIVATE flux)

add_executable(notes_demo notes/main.cpp)
target_link_libraries(notes_demo PRIVATE flux)

set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/llama.cpp
                 ${CMAKE_BINARY_DIR}/vendor/llama.cpp)

add_executable(llm_studio llm-studio/main.cpp)
# common transitively provides llama + ggml + Threads and all include dirs.
target_link_libraries(llm_studio PRIVATE flux common)

set(FLUX_EXAMPLE_TARGETS
    hello_world
    clock_demo
    blend_demo
    text_demo
    typography_demo
    image_demo
    scene_demo
    reactive_demo
    card_demo
    scroll_demo
    focus_demo
    request_focus_demo
    memo_demo
    cursor_demo
    foreach_demo
    hover_demo
    overlay_confirm_demo
    alert_demo
    actions_editor_demo
    textinput_demo
    textarea_demo
    markdown_styler_demo
    button_demo
    toggle_demo
    checkbox_demo
    slider_demo
    popover_demo
    tooltip_demo
    picker_demo
    icon_demo
    theme_demo
    layout_demo
    notes_demo
    llm_studio
)
foreach(t ${FLUX_EXAMPLE_TARGETS})
  flux_copy_example_resources(${t})
endforeach()
