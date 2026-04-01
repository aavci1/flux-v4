# UI test harness apps (FLUX_BUILD_UI_TESTS=ON). Each binary is driven by tests/ui/run_tests.py via --test-mode.

function(flux_copy_ui_test_resources target_name)
  add_custom_command(
    TARGET ${target_name}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../../resources/fonts/MaterialSymbolsRounded.ttf"
            "$<TARGET_FILE_DIR:${target_name}>/fonts/MaterialSymbolsRounded.ttf"
    VERBATIM
    COMMENT "Copy bundled fonts for ${target_name}")
endfunction()

set(FLUX_UI_TEST_APPS
    test_layout
    test_button
    test_toggle_checkbox
    test_text_input
    test_slider
    test_focus
    test_scroll
    test_overlay)

foreach(name ${FLUX_UI_TEST_APPS})
  add_executable(${name} apps/${name}/main.cpp)
  target_link_libraries(${name} PRIVATE flux)
  flux_copy_ui_test_resources(${name})
endforeach()
