set(MESH_GEN_SOURCES
  src/MeshGeneration.cpp
  src/PrimitiveGenerators.cpp
)

add_library(mesh-gen STATIC ${MESH_GEN_SOURCES})
target_compile_features(mesh-gen PUBLIC cxx_std_23)
target_include_directories(mesh-gen
  PUBLIC include
  PRIVATE src
)
target_link_libraries(mesh-gen PRIVATE sandbox::warnings_as_errors)

if(NOT SANDBOX_NATIVE_MESH_ONLY)
  foreach(configuration IN ITEMS Debug DebugGame Development Shipping Test)
    file(MAKE_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/MeshGen/${UE_PLATFORM}/${configuration}"
    )
  endforeach()

  add_library(sandbox-mesh-gen STATIC ${MESH_GEN_SOURCES})
  target_compile_features(sandbox-mesh-gen PUBLIC cxx_std_23)
  target_include_directories(sandbox-mesh-gen
    PUBLIC include
    PRIVATE src
  )
  target_link_libraries(sandbox-mesh-gen PRIVATE sandbox::warnings_as_errors)
  set_target_properties(sandbox-mesh-gen PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/MeshGen/${UE_PLATFORM}/${UE_CONFIGURATION}"
    OUTPUT_NAME sandbox_mesh_gen
  )

  if(WIN32)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
       NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
      message(FATAL_ERROR "sandbox-mesh-gen requires an MSVC-compatible ABI on Windows.")
    endif()

    set_target_properties(sandbox-mesh-gen PROPERTIES
      MSVC_RUNTIME_LIBRARY MultiThreadedDLL
    )
    target_compile_definitions(sandbox-mesh-gen PRIVATE NDEBUG _ITERATOR_DEBUG_LEVEL=0)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
      target_compile_options(sandbox-mesh-gen PRIVATE /U_DEBUG)
    else()
      target_compile_options(sandbox-mesh-gen PRIVATE -U_DEBUG)
    endif()
  endif()
endif()
