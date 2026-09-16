add_library(native-profiling-api INTERFACE)
add_library(sandbox::profiling-api ALIAS native-profiling-api)
target_include_directories(native-profiling-api INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(SANDBOX_WITH_TRACY)
  add_library(native-profiling STATIC)
  add_library(sandbox::profiling ALIAS native-profiling)
  target_sources(native-profiling PRIVATE src/memory.cpp)
  target_compile_features(native-profiling PUBLIC cxx_std_23)
  target_link_libraries(native-profiling
    PUBLIC native-profiling-api
    PRIVATE Tracy::TracyClient sandbox::warnings_as_errors
  )
  set_property(TARGET native-profiling PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
endif()
