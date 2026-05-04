# macOS app bundle helper for Flux applications.

function(flux_add_app target)
  cmake_parse_arguments(FA
    ""
    "APP_NAME;BUNDLE_IDENTIFIER;VERSION;BUILD;COPYRIGHT;CATEGORY;MIN_MACOS;ICON;ENTITLEMENTS"
    "RESOURCES;FONTS"
    ${ARGN})

  if(NOT FA_APP_NAME OR NOT FA_BUNDLE_IDENTIFIER)
    message(FATAL_ERROR "flux_add_app(${target}): APP_NAME and BUNDLE_IDENTIFIER are required")
  endif()

  if(NOT FA_VERSION)
    set(FA_VERSION "1.0")
  endif()
  if(NOT FA_BUILD)
    set(FA_BUILD "1")
  endif()
  if(NOT FA_COPYRIGHT)
    string(TIMESTAMP _flux_year "%Y")
    set(FA_COPYRIGHT "Copyright (c) ${_flux_year}")
  endif()
  if(NOT FA_CATEGORY)
    set(FA_CATEGORY "public.app-category.utilities")
  endif()
  if(NOT FA_MIN_MACOS)
    set(FA_MIN_MACOS "12.0")
  endif()
  if(NOT FA_ENTITLEMENTS)
    set(FA_ENTITLEMENTS "${PROJECT_SOURCE_DIR}/cmake/sandbox.entitlements")
  endif()

  set(_bundle_icon_file "")
  set(_generated_sources "")
  if(FA_ICON)
    if("${FA_ICON}" MATCHES "\\.icns$")
      set(_icns "${FA_ICON}")
    elseif("${FA_ICON}" MATCHES "\\.iconset$")
      set(_icns "${CMAKE_CURRENT_BINARY_DIR}/${target}.icns")
      add_custom_command(OUTPUT "${_icns}"
        COMMAND iconutil -c icns -o "${_icns}" "${FA_ICON}"
        DEPENDS "${FA_ICON}"
        VERBATIM
        COMMENT "Build ${target} app icon")
    elseif("${FA_ICON}" MATCHES "\\.svg$")
      find_program(_flux_qlmanage qlmanage)
      find_program(_flux_sips sips)
      find_program(_flux_iconutil iconutil)
      if(NOT _flux_qlmanage OR NOT _flux_sips OR NOT _flux_iconutil)
        message(FATAL_ERROR
          "flux_add_app(${target}): SVG icons require qlmanage, sips, and iconutil")
      endif()
      set(_icns "${CMAKE_CURRENT_BINARY_DIR}/${target}.icns")
      set(_iconset "${CMAKE_CURRENT_BINARY_DIR}/${target}.iconset")
      set(_icon_work_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}-icon-work")
      add_custom_command(OUTPUT "${_icns}"
        COMMAND "${CMAKE_COMMAND}"
          "-DINPUT=${FA_ICON}"
          "-DOUTPUT=${_icns}"
          "-DICONSET=${_iconset}"
          "-DWORK_DIR=${_icon_work_dir}"
          "-DQLMANAGE=${_flux_qlmanage}"
          "-DSIPS=${_flux_sips}"
          "-DICONUTIL=${_flux_iconutil}"
          -P "${PROJECT_SOURCE_DIR}/cmake/make-icns-from-svg.cmake"
        DEPENDS "${FA_ICON}" "${PROJECT_SOURCE_DIR}/cmake/make-icns-from-svg.cmake"
        VERBATIM
        COMMENT "Build ${target} app icon from SVG")
    else()
      message(FATAL_ERROR "flux_add_app(${target}): ICON must be a .icns, .iconset, or .svg path")
    endif()
    list(APPEND _generated_sources "${_icns}")
    get_filename_component(_bundle_icon_file "${_icns}" NAME)
  endif()

  set(MACOSX_BUNDLE_EXECUTABLE_NAME "${FA_APP_NAME}")
  set(MACOSX_BUNDLE_ICON_FILE "${_bundle_icon_file}")
  set(MACOSX_BUNDLE_GUI_IDENTIFIER "${FA_BUNDLE_IDENTIFIER}")
  set(MACOSX_BUNDLE_BUNDLE_NAME "${FA_APP_NAME}")
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${FA_BUILD}")
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${FA_VERSION}")
  set(MACOSX_BUNDLE_COPYRIGHT "${FA_COPYRIGHT}")
  configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/Info.plist.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist"
    @ONLY)

  add_executable(${target} MACOSX_BUNDLE ${FA_UNPARSED_ARGUMENTS} ${_generated_sources})
  target_link_libraries(${target} PRIVATE flux)

  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME "${FA_APP_NAME}"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "${FA_APP_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${FA_BUNDLE_IDENTIFIER}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${FA_BUILD}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${FA_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "${FA_COPYRIGHT}"
    XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "${FA_MIN_MACOS}"
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${FA_ENTITLEMENTS}")

  if(APPLE)
    foreach(_flux_app_dep IN ITEMS flux libtess2)
      if(TARGET ${_flux_app_dep})
        set_target_properties(${_flux_app_dep} PROPERTIES OSX_ARCHITECTURES "arm64;x86_64")
      endif()
    endforeach()
    set_target_properties(${target} PROPERTIES OSX_ARCHITECTURES "arm64;x86_64")
  endif()

  if(FA_ICON)
    set_source_files_properties("${_icns}" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources"
      GENERATED TRUE)
    set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_ICON_FILE "${_bundle_icon_file}")
  endif()

  foreach(_font IN LISTS FA_FONTS)
    target_sources(${target} PRIVATE "${_font}")
    set_source_files_properties("${_font}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts")
  endforeach()

  foreach(_res IN LISTS FA_RESOURCES)
    target_sources(${target} PRIVATE "${_res}")
    set_source_files_properties("${_res}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  endforeach()

  if(EXISTS "${FA_ENTITLEMENTS}")
    target_sources(${target} PRIVATE "${FA_ENTITLEMENTS}")
    set_source_files_properties("${FA_ENTITLEMENTS}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  endif()

  if(FLUX_ENABLE_ASAN)
    target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address)
  endif()

  add_custom_target(${target}-package
    COMMAND /bin/bash -c
            "\"${PROJECT_SOURCE_DIR}/cmake/sign-and-package.sh\" \"$<TARGET_BUNDLE_DIR:${target}>\" \"$FLUX_SIGN_APP_ID\" \"$FLUX_SIGN_INSTALLER_ID\" \"${CMAKE_CURRENT_BINARY_DIR}/${FA_APP_NAME}.pkg\""
    DEPENDS ${target}
    VERBATIM
    COMMENT "Sign and package ${FA_APP_NAME}")
endfunction()
