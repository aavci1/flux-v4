# Sample apps (link the static `flux` library). Included when FLUX_BUILD_EXAMPLES=ON at the project root (default OFF).

add_executable(hello_world hello-world/main.cpp)
target_link_libraries(hello_world PRIVATE flux)

add_executable(clock_demo clock-demo/main.cpp)
target_link_libraries(clock_demo PRIVATE flux)

add_executable(blend_demo blend-demo/main.cpp)
target_link_libraries(blend_demo PRIVATE flux)

add_executable(text_demo text-demo/main.cpp)
target_link_libraries(text_demo PRIVATE flux)

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

add_executable(button_demo button-demo/main.cpp)
target_link_libraries(button_demo PRIVATE flux)
