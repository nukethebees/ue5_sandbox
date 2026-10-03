add_library(jobserver-core STATIC)
target_sources(jobserver-core PRIVATE
  src/protocol.cpp
)
target_include_directories(jobserver-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(jobserver-core PUBLIC ioj::cxx_standard ioj::cxx_conformance)
ioj_enable_warnings(jobserver-core)
