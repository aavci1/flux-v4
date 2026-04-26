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
  enable_language(OBJC)
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
  OFF)

option(FLUX_ENABLE_ASAN
  "Build flux and flux_tests with AddressSanitizer instrumentation."
  OFF)

option(FLUX_V5_PROTOTYPE
  "Build the standalone Flux v5 reactive prototype."
  OFF)

if(FLUX_V5_PROTOTYPE)
  enable_testing()
  add_subdirectory(prototype/reactive)
endif()

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
  src/Reactive/Observer.cpp
  src/Reactive/Easing.cpp
  src/Reactive/Transition.cpp
  src/Reactive2/Signal.cpp
  src/Reactive2/Computed.cpp
  src/Reactive2/Effect.cpp
  src/Reactive2/Scope.cpp
  src/Reactive2/ObserverStack.cpp
  src/Core/ComponentKey.cpp
  src/Core/ComponentKeyUtil.cpp
  src/Core/Window.cpp
  src/Core/WindowRender.cpp
  src/UI/MeasureContext.cpp
  src/UI/Detail/TraversalContext.cpp
  src/UI/Environment.cpp
  src/UI/Theme.cpp
  src/UI/Element/ElementMeasurement.cpp
  src/UI/Element/ElementModifiers.cpp
  src/UI/Element/ElementTypeErasure.cpp
  src/UI/MountContext.cpp
  src/UI/MountRoot.cpp
  src/UI/Runtime.cpp
  src/UI/Overlay.cpp
  src/UI/Views/Rectangle.cpp
  src/UI/Views/Text.cpp
  src/UI/Views/LayoutContainers.cpp
  src/UI/LayoutDebugDump.cpp
  src/SceneGraph/SceneNode.cpp
  src/SceneGraph/GroupNode.cpp
  src/SceneGraph/RectNode.cpp
  src/SceneGraph/TextNode.cpp
  src/SceneGraph/ImageNode.cpp
  src/SceneGraph/PathNode.cpp
  src/SceneGraph/RenderNode.cpp
  src/SceneGraph/SceneGraph.cpp
  src/SceneGraph/SceneInteraction.cpp
  src/SceneGraph/SceneRenderer.cpp
  src/Graphics/Styles.cpp
  src/Graphics/AttributedString.cpp
  src/Graphics/Canvas.cpp
  src/Graphics/TextCacheDebugOverlay.cpp
  src/Graphics/Metal/MetalFrameRecorder.mm
  src/Graphics/TextSystem.cpp
  src/Graphics/xxhash_impl.c
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
  src/Platform/Mac/MacClipboard.mm
)
add_dependencies(flux flux_shaders)

target_include_directories(flux PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(flux PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_compile_options(flux PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

if(APPLE)
  target_compile_options(flux PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
endif()

if(FLUX_ENABLE_ASAN)
  target_compile_options(flux PRIVATE -fsanitize=address -fno-omit-frame-pointer)
  target_link_options(flux PRIVATE -fsanitize=address)
endif()

if(FLUX_ENABLE_DEFAULT_EVENT_LOGGING)
  target_compile_definitions(flux PRIVATE FLUX_ENABLE_DEFAULT_EVENT_LOGGING=1)
else()
  target_compile_definitions(flux PRIVATE FLUX_ENABLE_DEFAULT_EVENT_LOGGING=0)
endif()

option(FLUX_DISABLE_VARIANT_REFS
  "Deep-copy paragraph cache layouts (revert to clone on fast path); disables variant shared_ptr retention."
  OFF)
if(FLUX_DISABLE_VARIANT_REFS)
  target_compile_definitions(flux PRIVATE FLUX_DISABLE_VARIANT_REFS=1)
endif()

option(FLUX_ENABLE_PARAGRAPH_CACHE_PARALLEL_ASSERT
  "Debug: compare incremental vs full paragraph-cache assembly (non-NDEBUG only)."
  OFF)
if(FLUX_ENABLE_PARAGRAPH_CACHE_PARALLEL_ASSERT)
  target_compile_definitions(flux PRIVATE FLUX_PARAGRAPH_CACHE_PARALLEL_ASSERT=1)
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
  "-framework CoreVideo"
)

option(FLUX_BUILD_TESTS
  "Build unit and integration tests under tests/."
  OFF)

if(FLUX_BUILD_TESTS)
  # doctest's CMakeLists may require a minimum policy on CMake 4+.
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
  if(POLICY CMP0127)
    cmake_policy(SET CMP0127 NEW)
  endif()
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE
  )
  set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
  FetchContent_MakeAvailable(doctest)

  enable_testing()

  add_executable(flux_tests
    tests/AlertTests.cpp
    tests/AnimationTests.cpp
    tests/ForTests.cpp
    tests/MetalCanvasTests.mm
    tests/MountRootTests.cpp
    tests/main.cpp
    tests/SceneGraphTests.cpp
    tests/ParagraphCacheTests.cpp
    tests/ParagraphCacheIncrementalTests.cpp
    tests/RuntimeTests.cpp
    tests/ShowTests.cpp
    tests/ThemeReactivityTests.cpp
  )

  target_include_directories(flux_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(flux_tests PRIVATE
    flux
    doctest::doctest
  )

  set_target_properties(flux_tests PROPERTIES CXX_STANDARD 23)

  if(FLUX_ENABLE_ASAN)
    target_compile_options(flux_tests PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(flux_tests PRIVATE -fsanitize=address)
  endif()

  add_test(NAME flux_tests COMMAND flux_tests)

  add_executable(flux_reactive2_tests
    tests/Reactive2/main.cpp
    tests/Reactive2/SignalTests.cpp
    tests/Reactive2/ComputedTests.cpp
    tests/Reactive2/EffectTests.cpp
    tests/Reactive2/ScopeTests.cpp
    tests/Reactive2/DiamondTests.cpp
    tests/Reactive2/BindableTests.cpp
  )

  target_include_directories(flux_reactive2_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(flux_reactive2_tests PRIVATE
    flux
    doctest::doctest
  )

  set_target_properties(flux_reactive2_tests PROPERTIES CXX_STANDARD 23)

  if(FLUX_ENABLE_ASAN)
    target_compile_options(flux_reactive2_tests PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(flux_reactive2_tests PRIVATE -fsanitize=address)
  endif()

  add_test(NAME flux_reactive2_tests COMMAND flux_reactive2_tests)
endif()

option(FLUX_BUILD_EXAMPLES
  "Build standalone examples under examples/."
  OFF)

if(FLUX_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

option(FLUX_BUILD_BENCHMARKS "Build micro-benchmarks in bench/." OFF)

if(FLUX_BUILD_BENCHMARKS)
  add_executable(paragraph_cache_bench
    bench/paragraph_cache/bench.mm
  )
  target_include_directories(paragraph_cache_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_link_libraries(paragraph_cache_bench PRIVATE flux)
  set_target_properties(paragraph_cache_bench PROPERTIES CXX_STANDARD 23)
  if(APPLE)
    target_compile_options(paragraph_cache_bench PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  endif()

endif()
