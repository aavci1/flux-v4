cmake_minimum_required(VERSION 3.25)

project(flux VERSION 4.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Planned backends: LINUX_WAYLAND, LINUX_KMS — reserved for future CMake branches.
set(FLUX_PLATFORM "AUTO" CACHE STRING
    "Flux backend: AUTO, MACOS, LINUX_WAYLAND, LINUX_KMS (only MACOS is implemented).")

if(FLUX_PLATFORM STREQUAL "AUTO")
  if(APPLE)
    set(FLUX_PLATFORM_MACOS ON)
    set(FLUX_PLATFORM "MACOS" CACHE STRING
      "Flux backend: AUTO, MACOS, LINUX_WAYLAND, LINUX_KMS (only MACOS is implemented)." FORCE)
  else()
    message(FATAL_ERROR
      "Flux: non-macOS builds are not implemented yet (planned: Linux Wayland/Vulkan, Linux KMS/DRM).")
  endif()
elseif(FLUX_PLATFORM STREQUAL "MACOS")
  if(NOT APPLE)
    message(FATAL_ERROR "FLUX_PLATFORM=MACOS requires an Apple host.")
  endif()
  set(FLUX_PLATFORM_MACOS ON)
else()
  message(FATAL_ERROR "Flux: FLUX_PLATFORM=${FLUX_PLATFORM} is not implemented yet.")
endif()

if(FLUX_PLATFORM_MACOS)
  enable_language(OBJCXX)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

include(FetchContent)
# libtess2 — path fill/stroke tessellation (same stack as upstream flux)
FetchContent_Declare(libtess2
  GIT_REPOSITORY https://github.com/memononen/libtess2.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(libtess2)
if(NOT TARGET libtess2)
  add_library(libtess2 STATIC
    ${libtess2_SOURCE_DIR}/Source/bucketalloc.c
    ${libtess2_SOURCE_DIR}/Source/dict.c
    ${libtess2_SOURCE_DIR}/Source/geom.c
    ${libtess2_SOURCE_DIR}/Source/mesh.c
    ${libtess2_SOURCE_DIR}/Source/priorityq.c
    ${libtess2_SOURCE_DIR}/Source/sweep.c
    ${libtess2_SOURCE_DIR}/Source/tess.c
  )
  set_target_properties(libtess2 PROPERTIES LINKER_LANGUAGE C)
endif()
set_target_properties(libtess2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
target_include_directories(libtess2 PUBLIC
  $<BUILD_INTERFACE:${libtess2_SOURCE_DIR}/Include>
)

option(FLUX_ENABLE_DEFAULT_EVENT_LOGGING
  "Print built-in Application event handlers to stdout (disable for quiet or embedded builds)."
  ON)

# Offline Metal: compile CanvasShaders.metal → air → metallib → C header (xxd) embedded in MetalShaderLibrary.mm
set(FLUX_METAL_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Metal/CanvasShaders.metal)
set(FLUX_METAL_AIR ${CMAKE_CURRENT_BINARY_DIR}/CanvasShaders.air)
set(FLUX_METALLIB ${CMAKE_CURRENT_BINARY_DIR}/FluxShaders.metallib)
set(FLUX_METALLIB_EMBED_H ${CMAKE_CURRENT_BINARY_DIR}/FluxShaders.metallib.h)
# Run xxd in the build dir with basename only so symbols are FluxShaders_metallib (not full path).
add_custom_command(
  OUTPUT ${FLUX_METAL_AIR} ${FLUX_METALLIB} ${FLUX_METALLIB_EMBED_H}
  COMMAND xcrun -sdk macosx metal -mmacosx-version-min=11.0 -std=metal3.0 -c ${FLUX_METAL_SOURCE} -o ${FLUX_METAL_AIR}
  COMMAND xcrun -sdk macosx metallib ${FLUX_METAL_AIR} -o ${FLUX_METALLIB}
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
          xxd -i FluxShaders.metallib FluxShaders.metallib.h
  DEPENDS ${FLUX_METAL_SOURCE}
  COMMENT "Compile CanvasShaders.metal to FluxShaders.metallib (embedded)"
  VERBATIM
)
add_custom_target(flux_shaders DEPENDS ${FLUX_METALLIB_EMBED_H})

add_library(flux STATIC
  src/Core/Application.mm
  src/Core/EventQueue.cpp
  src/Core/Window.cpp
  src/Graphics/Canvas.cpp
  src/Graphics/TextSystem.cpp
  src/Graphics/CoreTextSystem.mm
  src/Graphics/Path.cpp
  src/Graphics/PathFlattener.cpp
  src/Graphics/Metal/GlyphAtlas.mm
  src/Graphics/Metal/MetalImage.mm
  src/Graphics/Metal/MetalPathRasterizer.cpp
  src/Graphics/Metal/MetalShaderLibrary.mm
  src/Graphics/Metal/MetalDeviceResources.mm
  src/Graphics/Metal/MetalCanvas.mm
  src/Platform/Mac/MacMetalWindow.mm
)
add_dependencies(flux flux_shaders)

target_include_directories(flux PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(flux PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(flux PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

if(APPLE)
  target_compile_options(flux PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
endif()

if(FLUX_ENABLE_DEFAULT_EVENT_LOGGING)
  target_compile_definitions(flux PRIVATE FLUX_ENABLE_DEFAULT_EVENT_LOGGING=1)
else()
  target_compile_definitions(flux PRIVATE FLUX_ENABLE_DEFAULT_EVENT_LOGGING=0)
endif()

target_compile_definitions(flux PRIVATE FLUX_PLATFORM_MACOS=1)

target_link_libraries(flux PRIVATE
  libtess2
  "-framework Cocoa"
  "-framework QuartzCore"
  "-framework Metal"
  "-framework MetalKit"
  "-framework Foundation"
  "-framework CoreText"
)

add_executable(hello_world examples/hello-world/main.cpp)
target_link_libraries(hello_world PRIVATE flux)

add_executable(clock_demo examples/clock-demo/main.cpp)
target_link_libraries(clock_demo PRIVATE flux)

add_executable(blend_demo examples/blend-demo/main.cpp)
target_link_libraries(blend_demo PRIVATE flux)

add_executable(text_demo examples/text-demo/main.cpp)
target_link_libraries(text_demo PRIVATE flux)

add_executable(image_demo examples/image-demo/main.cpp)
target_link_libraries(image_demo PRIVATE flux)
