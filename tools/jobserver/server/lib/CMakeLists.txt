add_library(jobserver-board STATIC)
target_sources(jobserver-board PRIVATE
  src/board.cpp
  include/jobserver/server/board.hpp
  include/jobserver/server/ticket.hpp
)
target_include_directories(jobserver-board PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(jobserver-board PUBLIC jobserver-core nlohmann_json::nlohmann_json)
ioj_enable_warnings(jobserver-board)

if(NOT TARGET jobserver-platform)
  return()
endif()

add_library(jobserver-server STATIC)
target_sources(jobserver-server PRIVATE
  src/server.cpp
  include/jobserver/server/server.hpp
)
target_link_libraries(jobserver-server PUBLIC jobserver-board PRIVATE jobserver-platform)
ioj_enable_warnings(jobserver-server)
