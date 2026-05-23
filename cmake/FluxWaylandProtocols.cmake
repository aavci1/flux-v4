# FluxWaylandProtocols.cmake — generate Wayland protocol sources for flux (client) and
# lambda-window-manager (server) from XML. Avoids compiling duplicate checked-in protocol .c
# files when generated equivalents exist.

function(flux_wayland_protocols)
  cmake_parse_arguments(_flux_wp
    ""
    "TARGET;MODE;XML;BASENAME"
    ""
    ${ARGN})

  if(NOT _flux_wp_TARGET OR NOT _flux_wp_MODE OR NOT _flux_wp_XML OR NOT _flux_wp_BASENAME)
    message(FATAL_ERROR
      "flux_wayland_protocols requires TARGET, MODE (CLIENT|SERVER|BOTH), XML, and BASENAME")
  endif()

  if(NOT EXISTS "${_flux_wp_XML}")
    message(FATAL_ERROR "flux_wayland_protocols: XML not found: ${_flux_wp_XML}")
  endif()

  if(NOT WAYLAND_SCANNER)
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
  endif()

  if(NOT FLUX_WAYLAND_PROTOCOL_DIR)
    set(FLUX_WAYLAND_PROTOCOL_DIR ${CMAKE_CURRENT_BINARY_DIR}/wayland-protocols PARENT_SCOPE)
    file(MAKE_DIRECTORY ${FLUX_WAYLAND_PROTOCOL_DIR})
  endif()

  set(_flux_wp_out_dir ${FLUX_WAYLAND_PROTOCOL_DIR})
  set(_flux_wp_outputs)
  set(_flux_wp_commands)

  if(_flux_wp_MODE STREQUAL "CLIENT" OR _flux_wp_MODE STREQUAL "BOTH")
    set(_flux_wp_client_h ${_flux_wp_out_dir}/${_flux_wp_BASENAME}-client-protocol.h)
    set(_flux_wp_client_c ${_flux_wp_out_dir}/${_flux_wp_BASENAME}-protocol.c)
    list(APPEND _flux_wp_outputs ${_flux_wp_client_h} ${_flux_wp_client_c})
    list(APPEND _flux_wp_commands
      COMMAND ${WAYLAND_SCANNER} client-header ${_flux_wp_XML} ${_flux_wp_client_h}
      COMMAND ${WAYLAND_SCANNER} private-code ${_flux_wp_XML} ${_flux_wp_client_c})
  endif()

  if(_flux_wp_MODE STREQUAL "SERVER" OR _flux_wp_MODE STREQUAL "BOTH")
    set(_flux_wp_server_h ${_flux_wp_out_dir}/${_flux_wp_BASENAME}-server-protocol.h)
    set(_flux_wp_server_c ${_flux_wp_out_dir}/${_flux_wp_BASENAME}-protocol.c)
    list(APPEND _flux_wp_outputs ${_flux_wp_server_h} ${_flux_wp_server_c})
    list(APPEND _flux_wp_commands
      COMMAND ${WAYLAND_SCANNER} server-header ${_flux_wp_XML} ${_flux_wp_server_h}
      COMMAND ${WAYLAND_SCANNER} private-code ${_flux_wp_XML} ${_flux_wp_server_c})
  endif()

  set(_flux_wp_stamp ${_flux_wp_out_dir}/${_flux_wp_BASENAME}-${_flux_wp_MODE}.stamp)
  add_custom_command(
    OUTPUT ${_flux_wp_outputs} ${_flux_wp_stamp}
    ${_flux_wp_commands}
    COMMAND ${CMAKE_COMMAND} -E touch ${_flux_wp_stamp}
    DEPENDS ${_flux_wp_XML}
    COMMENT "Generating Wayland protocol ${_flux_wp_BASENAME} (${_flux_wp_MODE})"
    VERBATIM
  )

  target_sources(${_flux_wp_TARGET} PRIVATE ${_flux_wp_outputs})
  set_source_files_properties(${_flux_wp_outputs} PROPERTIES GENERATED TRUE)

  if(_flux_wp_MODE STREQUAL "CLIENT" OR _flux_wp_MODE STREQUAL "BOTH")
    set(_flux_wp_client_sources ${_flux_wp_client_c} PARENT_SCOPE)
  endif()
  if(_flux_wp_MODE STREQUAL "SERVER" OR _flux_wp_MODE STREQUAL "BOTH")
    set(_flux_wp_server_sources ${_flux_wp_server_c} PARENT_SCOPE)
  endif()
endfunction()
