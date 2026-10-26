add_library(native-profiling-api INTERFACE)
add_library(sandbox::profiling-api ALIAS native-profiling-api)
target_include_directories(native-profiling-api INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(IOJ_WITH_TRACY)
  add_library(native-profiling INTERFACE)
  add_library(sandbox::profiling ALIAS native-profiling)
  target_sources(TracyClient
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src/memory.cpp"
  )
  target_compile_features(TracyClient PRIVATE cxx_std_23)
  target_link_libraries(TracyClient
    PRIVATE
      native-profiling-api
      sandbox::native_no_rtti
  )
  target_compile_definitions(TracyClient PRIVATE IOJ_TRACY_MEMORY_EXPORTS)
  target_link_libraries(native-profiling
    INTERFACE
      native-profiling-api
      Tracy::TracyClient
  )
endif()
