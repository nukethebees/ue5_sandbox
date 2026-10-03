if(NOT TARGET jobserver-platform)
  return()
endif()

add_library(jobserver-client STATIC)
add_library(jobserver::client ALIAS jobserver-client)
target_sources(jobserver-client PRIVATE
  src/request.cpp
  include/jobserver/client/request.hpp
)
target_include_directories(jobserver-client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(jobserver-client PUBLIC jobserver-platform nlohmann_json::nlohmann_json)
ioj_enable_warnings(jobserver-client)
