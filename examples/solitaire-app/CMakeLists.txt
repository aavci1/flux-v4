if(APPLE)
  flux_add_app(solitaire-app main.cpp
    APP_NAME "Solitaire"
    BUNDLE_IDENTIFIER "com.aavci1.solitaire"
    VERSION "1.0"
    BUILD "1"
    COPYRIGHT "Copyright (c) 2026 Abdurrahman Avci"
    CATEGORY "public.app-category.card-games"
    ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.svg"
    FONTS "${PROJECT_SOURCE_DIR}/resources/fonts/MaterialSymbolsRounded.ttf")
else()
  add_executable(solitaire-app main.cpp)
  target_link_libraries(solitaire-app PRIVATE flux)
  add_custom_command(
    TARGET solitaire-app
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:solitaire-app>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/resources/fonts/MaterialSymbolsRounded.ttf"
            "$<TARGET_FILE_DIR:solitaire-app>/fonts/MaterialSymbolsRounded.ttf"
    VERBATIM
    COMMENT "Copy bundled fonts for solitaire-app")
endif()
