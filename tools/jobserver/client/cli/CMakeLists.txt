add_library(jobserver-cli STATIC)
target_sources(jobserver-cli PRIVATE commands.cpp commands.hpp)
target_include_directories(jobserver-cli PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(jobserver-cli
  PUBLIC jobserver-core nlohmann_json::nlohmann_json
  PRIVATE CLI11::CLI11
)
ioj_enable_warnings(jobserver-cli)

if(NOT TARGET jobserver-client)
  return()
endif()

add_executable(jobserver)
target_sources(jobserver PRIVATE ../../platform/windows/cli_main.cpp)
target_link_libraries(jobserver PRIVATE jobserver-cli jobserver::client)
ioj_enable_warnings(jobserver)
