add_library(material-gen STATIC)
target_sources(material-gen PRIVATE
  src/CompiledMaterial.cpp
  src/MaterialFrontend.cpp
  src/MaterialIR.cpp
  src/SourceHash.cpp
)
target_compile_features(material-gen PUBLIC cxx_std_23)
target_include_directories(material-gen PUBLIC include)
target_link_libraries(material-gen
  PRIVATE
    lispb-syntax
    sandbox::codegen_warnings
)

if(NOT SANDBOX_CODEGEN_ONLY)
  foreach(configuration IN ITEMS Debug DebugGame Development Shipping Test)
    file(MAKE_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/MaterialGen/${UE_PLATFORM}/${configuration}"
    )
  endforeach()

  add_library(sandbox-material-gen STATIC)
  target_sources(sandbox-material-gen PRIVATE
    src/CompiledMaterial.cpp
    src/MaterialIR.cpp
    src/SourceHash.cpp
  )
  target_compile_features(sandbox-material-gen PUBLIC cxx_std_23)
  target_include_directories(sandbox-material-gen PUBLIC include)
  target_link_libraries(sandbox-material-gen PRIVATE sandbox::codegen_warnings)
  set_target_properties(sandbox-material-gen PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/MaterialGen/${UE_PLATFORM}/${UE_CONFIGURATION}"
    OUTPUT_NAME sandbox_material_gen
  )

  if(WIN32)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
       NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
      message(FATAL_ERROR "sandbox-material-gen requires an MSVC-compatible ABI on Windows.")
    endif()

    set_target_properties(sandbox-material-gen PROPERTIES
      MSVC_RUNTIME_LIBRARY MultiThreadedDLL
    )
    target_compile_definitions(sandbox-material-gen PRIVATE NDEBUG _ITERATOR_DEBUG_LEVEL=0)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
      target_compile_options(sandbox-material-gen PRIVATE /U_DEBUG)
    else()
      target_compile_options(sandbox-material-gen PRIVATE -U_DEBUG)
    endif()
  endif()
endif()
