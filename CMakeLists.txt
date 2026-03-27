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

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Metal/CanvasShaders.metal SHADER_SRC)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Metal/ShaderSource.cpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/MetalShaderSource.cpp
  @ONLY
)

add_library(flux STATIC
  src/Core/Application.mm
  src/Core/EventQueue.cpp
  src/Core/Window.cpp
  src/Graphics/Canvas.cpp
  src/Graphics/Path.cpp
  src/Graphics/PathFlattener.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/MetalShaderSource.cpp
  src/Graphics/Metal/MetalCanvas.mm
  src/Platform/Mac/MacMetalWindow.mm
)

target_include_directories(flux PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(flux PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
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
  "-framework Foundation"
)

add_executable(hello_world examples/hello-world/main.cpp)
target_link_libraries(hello_world PRIVATE flux)

add_executable(clock_demo examples/clock-demo/main.cpp)
target_link_libraries(clock_demo PRIVATE flux)
