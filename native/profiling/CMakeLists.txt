add_library(native-profiling-api INTERFACE)
add_library(sandbox::profiling-api ALIAS native-profiling-api)
target_include_directories(native-profiling-api INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(IOJ_WITH_TRACY)
  add_library(native-profiling STATIC)
  add_library(sandbox::profiling ALIAS native-profiling)
  target_sources(native-profiling PRIVATE src/memory.cpp)
  target_link_libraries(native-profiling
    PUBLIC
      sandbox::native_defaults
      native-profiling-api
    PRIVATE
      Tracy::TracyClient
  )
  ioj_enable_warnings(native-profiling)
  ioj_apply_native_abi(native-profiling)
endif()
