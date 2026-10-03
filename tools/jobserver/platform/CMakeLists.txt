if(NOT WIN32)
  message(STATUS "Jobserver: building portable libraries/tests; no transport backend for this OS")
  return()
endif()

add_library(jobserver-platform STATIC)
target_sources(jobserver-platform PRIVATE
  include/jobserver/platform/transport.hpp
  windows/server_transport.cpp
  windows/transport_common.cpp
  windows/transport_common.hpp
)
target_include_directories(jobserver-platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(jobserver-platform PUBLIC jobserver-core PRIVATE advapi32)
target_compile_definitions(jobserver-platform PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
ioj_enable_warnings(jobserver-platform)
