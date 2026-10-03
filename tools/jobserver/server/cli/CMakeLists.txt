if(NOT TARGET jobserver-server)
  return()
endif()

add_executable(jobserverd WIN32)
target_sources(jobserverd PRIVATE ../../platform/windows/daemon_main.cpp)
target_link_libraries(jobserverd PRIVATE jobserver-server jobserver-platform)
target_compile_definitions(jobserverd PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
ioj_enable_warnings(jobserverd)
