# Cross-platform helper for Lambda applications and demos.

function(lambda_copy_runtime_resources target_name)
  add_custom_command(
    TARGET ${target_name}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/resources/fonts/MaterialSymbolsRounded.ttf"
            "$<TARGET_FILE_DIR:${target_name}>/fonts/MaterialSymbolsRounded.ttf"
    VERBATIM
    COMMENT "Copy bundled fonts for ${target_name}")
endfunction()

function(lambda_add_app target)
  cmake_parse_arguments(LA
    ""
    "APP_NAME;BUNDLE_IDENTIFIER;VERSION;BUILD;COPYRIGHT;CATEGORY;MIN_MACOS;ICON;ENTITLEMENTS;COMMENT"
    "SOURCES;RESOURCES;FONTS"
    ${ARGN})

  if(NOT LA_APP_NAME)
    set(LA_APP_NAME "${target}")
  endif()
  if(NOT LA_BUNDLE_IDENTIFIER)
    string(REPLACE "-" "." _lambda_bundle_suffix "${target}")
    set(LA_BUNDLE_IDENTIFIER "io.lambda.${_lambda_bundle_suffix}")
  endif()
  if(NOT LA_VERSION)
    set(LA_VERSION "1.0")
  endif()
  if(NOT LA_BUILD)
    set(LA_BUILD "1")
  endif()
  if(NOT LA_COPYRIGHT)
    string(TIMESTAMP _lambda_year "%Y")
    set(LA_COPYRIGHT "Copyright (c) ${_lambda_year}")
  endif()
  if(NOT LA_CATEGORY)
    set(LA_CATEGORY "public.app-category.utilities")
  endif()
  if(NOT LA_MIN_MACOS)
    set(LA_MIN_MACOS "12.0")
  endif()
  if(NOT LA_ENTITLEMENTS)
    set(LA_ENTITLEMENTS "${PROJECT_SOURCE_DIR}/cmake/sandbox.entitlements")
  endif()
  if(NOT LA_COMMENT)
    set(LA_COMMENT "${LA_APP_NAME}")
  endif()

  set(_lambda_app_sources ${LA_SOURCES} ${LA_UNPARSED_ARGUMENTS})
  if(NOT _lambda_app_sources)
    message(FATAL_ERROR "lambda_add_app(${target}): no sources provided")
  endif()

  if(APPLE)
    set(_bundle_icon_file "")
    set(_generated_sources "")
    if(LA_ICON)
      if("${LA_ICON}" MATCHES "\\.icns$")
        set(_icns "${LA_ICON}")
      elseif("${LA_ICON}" MATCHES "\\.iconset$")
        set(_icns "${CMAKE_CURRENT_BINARY_DIR}/${target}.icns")
        add_custom_command(OUTPUT "${_icns}"
          COMMAND iconutil -c icns -o "${_icns}" "${LA_ICON}"
          DEPENDS "${LA_ICON}"
          VERBATIM
          COMMENT "Build ${target} app icon")
      elseif("${LA_ICON}" MATCHES "\\.svg$")
        find_program(_lambda_qlmanage qlmanage)
        find_program(_lambda_sips sips)
        find_program(_lambda_iconutil iconutil)
        if(NOT _lambda_qlmanage OR NOT _lambda_sips OR NOT _lambda_iconutil)
          message(FATAL_ERROR
            "lambda_add_app(${target}): SVG icons require qlmanage, sips, and iconutil")
        endif()
        set(_icns "${CMAKE_CURRENT_BINARY_DIR}/${target}.icns")
        set(_iconset "${CMAKE_CURRENT_BINARY_DIR}/${target}.iconset")
        set(_icon_work_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}-icon-work")
        add_custom_command(OUTPUT "${_icns}"
          COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${LA_ICON}"
            "-DOUTPUT=${_icns}"
            "-DICONSET=${_iconset}"
            "-DWORK_DIR=${_icon_work_dir}"
            "-DQLMANAGE=${_lambda_qlmanage}"
            "-DSIPS=${_lambda_sips}"
            "-DICONUTIL=${_lambda_iconutil}"
            -P "${PROJECT_SOURCE_DIR}/cmake/make-icns-from-svg.cmake"
          DEPENDS "${LA_ICON}" "${PROJECT_SOURCE_DIR}/cmake/make-icns-from-svg.cmake"
          VERBATIM
          COMMENT "Build ${target} app icon from SVG")
      else()
        message(FATAL_ERROR "lambda_add_app(${target}): ICON must be a .icns, .iconset, or .svg path")
      endif()
      list(APPEND _generated_sources "${_icns}")
      get_filename_component(_bundle_icon_file "${_icns}" NAME)
    endif()

    set(MACOSX_BUNDLE_EXECUTABLE_NAME "${LA_APP_NAME}")
    set(MACOSX_BUNDLE_ICON_FILE "${_bundle_icon_file}")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "${LA_BUNDLE_IDENTIFIER}")
    set(MACOSX_BUNDLE_BUNDLE_NAME "${LA_APP_NAME}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${LA_BUILD}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${LA_VERSION}")
    set(MACOSX_BUNDLE_COPYRIGHT "${LA_COPYRIGHT}")
    configure_file(
      "${PROJECT_SOURCE_DIR}/cmake/Info.plist.in"
      "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist"
      @ONLY)

    add_executable(${target} MACOSX_BUNDLE ${_lambda_app_sources} ${_generated_sources})
    set_target_properties(${target} PROPERTIES
      OUTPUT_NAME "${LA_APP_NAME}"
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist"
      MACOSX_BUNDLE_BUNDLE_NAME "${LA_APP_NAME}"
      MACOSX_BUNDLE_GUI_IDENTIFIER "${LA_BUNDLE_IDENTIFIER}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${LA_BUILD}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${LA_VERSION}"
      MACOSX_BUNDLE_COPYRIGHT "${LA_COPYRIGHT}"
      XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "${LA_MIN_MACOS}"
      XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${LA_ENTITLEMENTS}")

    foreach(_lambda_app_dep IN ITEMS lambda libtess2)
      if(TARGET ${_lambda_app_dep})
        set_target_properties(${_lambda_app_dep} PROPERTIES OSX_ARCHITECTURES "arm64;x86_64")
      endif()
    endforeach()
    set_target_properties(${target} PROPERTIES OSX_ARCHITECTURES "arm64;x86_64")

    if(LA_ICON)
      set_source_files_properties("${_icns}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
        GENERATED TRUE)
      set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_ICON_FILE "${_bundle_icon_file}")
    endif()
    foreach(_font IN LISTS LA_FONTS)
      target_sources(${target} PRIVATE "${_font}")
      set_source_files_properties("${_font}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts")
    endforeach()
    foreach(_res IN LISTS LA_RESOURCES)
      target_sources(${target} PRIVATE "${_res}")
      set_source_files_properties("${_res}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    endforeach()

    add_custom_target(${target}-package
      COMMAND /bin/bash -c
              "\"${PROJECT_SOURCE_DIR}/cmake/sign-and-package.sh\" \"$<TARGET_BUNDLE_DIR:${target}>\" \"$LAMBDA_SIGN_APP_ID\" \"$LAMBDA_SIGN_INSTALLER_ID\" \"${CMAKE_CURRENT_BINARY_DIR}/${LA_APP_NAME}.pkg\" \"${LA_ENTITLEMENTS}\""
      DEPENDS ${target}
      VERBATIM
      COMMENT "Sign and package ${LA_APP_NAME}")
  else()
    add_executable(${target} ${_lambda_app_sources})
    lambda_copy_runtime_resources(${target})

    string(REPLACE "." "-" _lambda_desktop_id "${LA_BUNDLE_IDENTIFIER}")
    set(_lambda_desktop_file "${CMAKE_CURRENT_BINARY_DIR}/${_lambda_desktop_id}.desktop")
    file(GENERATE OUTPUT "${_lambda_desktop_file}" CONTENT
"[Desktop Entry]
Type=Application
Name=${LA_APP_NAME}
Comment=${LA_COMMENT}
Exec=${target}
Icon=${target}
Categories=Utility;
Terminal=false
")
    install(TARGETS ${target} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES "${_lambda_desktop_file}" DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    if(LA_ICON)
      install(FILES "${LA_ICON}"
              DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
              RENAME "${target}.svg")
    endif()
  endif()

  target_link_libraries(${target} PRIVATE lambda)
  if(LAMBDA_ENABLE_ASAN)
    target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address)
  endif()
endfunction()
