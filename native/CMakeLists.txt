if(IOJ_ENABLE_ASAN)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

add_library(sandbox_native_no_rtti INTERFACE)
add_library(sandbox::native_no_rtti ALIAS sandbox_native_no_rtti)
target_compile_options(sandbox_native_no_rtti INTERFACE
  "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:/GR->"
  "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>>:-fno-rtti>"
)

add_library(sandbox_native_defaults INTERFACE)
add_library(sandbox::native_defaults ALIAS sandbox_native_defaults)
target_link_libraries(sandbox_native_defaults INTERFACE
  sandbox::native_no_rtti
  ioj::warnings_as_errors
  ioj::asan
)

add_library(sandbox_native_codegen_defaults INTERFACE)
add_library(sandbox::native_codegen_defaults ALIAS sandbox_native_codegen_defaults)
target_link_libraries(sandbox_native_codegen_defaults INTERFACE
  sandbox::native_no_rtti
  ioj::codegen_warnings
  ioj::asan
)

enable_language(C)

add_subdirectory(third_party)
add_subdirectory(third_party/handmade_math)

set(IOJ_WITH_TRACY OFF)
if(UE_CONFIGURATION MATCHES "^(Debug|DebugGame|Development)$")
  set(IOJ_WITH_TRACY ON)

  set(TRACY_ENABLE ON)
  set(TRACY_ON_DEMAND ON)
  set(TRACY_ONLY_LOCALHOST ON)
  set(TRACY_NO_BROADCAST ON)
  set(TRACY_NO_CRASH_HANDLER ON)
  set(TRACY_STATIC ON)
  set(TRACY_MANUAL_LIFETIME OFF)

  find_package(Threads REQUIRED)
  add_subdirectory(third_party/tracy EXCLUDE_FROM_ALL)
  set_property(TARGET TracyClient PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_compile_definitions(TracyClient PRIVATE _ITERATOR_DEBUG_LEVEL=0)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(TracyClient PRIVATE -Wno-format -Wno-unused-value)
  elseif(MSVC)
    target_compile_options(TracyClient PRIVATE /wd4477 /wd4552)
  endif()
  set_target_properties(TracyClient PROPERTIES OUTPUT_NAME SandboxTracyClientStatic)

  if(IOJ_WITH_UNREAL)
    add_library(TracyClientUnreal SHARED)
    target_sources(TracyClientUnreal PRIVATE
      third_party/tracy/public/TracyClient.cpp
      profiling/src/memory.cpp
    )
    target_compile_features(TracyClientUnreal PUBLIC cxx_std_11)
    target_include_directories(TracyClientUnreal PUBLIC profiling/include)
    target_include_directories(TracyClientUnreal SYSTEM PUBLIC third_party/tracy/public)
    get_target_property(tracy_definitions TracyClient INTERFACE_COMPILE_DEFINITIONS)
    get_target_property(tracy_libraries TracyClient INTERFACE_LINK_LIBRARIES)
    target_compile_definitions(TracyClientUnreal
      PUBLIC ${tracy_definitions} TRACY_MANUAL_LIFETIME TRACY_IMPORTS
      PRIVATE TRACY_EXPORTS IOJ_TRACY_MEMORY_EXPORTS _ITERATOR_DEBUG_LEVEL=0
    )
    target_link_libraries(TracyClientUnreal PUBLIC ${tracy_libraries})
    set_property(TARGET TracyClientUnreal PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(TracyClientUnreal PRIVATE -Wno-format -Wno-unused-value)
    elseif(MSVC)
      target_compile_options(TracyClientUnreal PRIVATE /wd4477 /wd4552)
    endif()
    set_target_properties(TracyClientUnreal PROPERTIES
      OUTPUT_NAME SandboxTracyClient
      ARCHIVE_OUTPUT_DIRECTORY
        "${PROJECT_SOURCE_DIR}/Binaries/Native/Tracy/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
      RUNTIME_OUTPUT_DIRECTORY
        "${PROJECT_SOURCE_DIR}/Binaries/Native/Tracy/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
    )
  endif()
endif()

if(IOJ_ENABLE_CLANG_TIDY)
  sandbox_configure_native_clang_tidy()
endif()

add_subdirectory(profiling)
add_subdirectory(sbx_mimalloc)
add_subdirectory(lispb)
add_subdirectory(layout)
add_subdirectory(memory)
add_subdirectory(s7)
add_subdirectory(core)
add_subdirectory(simulation)
add_subdirectory(level_authoring)
add_subdirectory(simulation_benchmark)
add_subdirectory(image)
add_subdirectory(mesh_gen)
add_subdirectory(shaders)

add_custom_target(native-tests
  DEPENDS
    codegen-tests
    kernel-codegen-tests
    native-core-tests
    native-soa-tests
    native-soa-tests-mimalloc
    native-layout-tests
    native-level-authoring-tests
    native-memory-tests
    native-simulation-tests
    native-simulation-benchmark-tests
    sandbox-image-tests
    sandbox-s7-tests
    sbx-mimalloc-tests
    slate-codegen-tests
    mesh-gen-tests
    check-generated-code
)
set_property(TARGET native-tests PROPERTY FOLDER "Sandbox/Native/Tests")

if(TARGET TracyClientUnreal)
  set_property(SOURCE profiling/src/memory.cpp APPEND PROPERTY
    COMPILE_OPTIONS "$<TARGET_PROPERTY:sandbox_native_no_rtti,INTERFACE_COMPILE_OPTIONS>"
  )
endif()

if(IOJ_ENABLE_CLANG_TIDY)
  sandbox_add_native_clang_tidy_target()
endif()
