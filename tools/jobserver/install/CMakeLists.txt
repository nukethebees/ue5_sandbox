if(NOT TARGET jobserver)
  return()
endif()

install(TARGETS jobserver jobserverd
  RUNTIME DESTINATION bin
  COMPONENT jobserver
)

add_custom_target(install-jobserver
  COMMAND pwsh -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/Install-Jobserver.ps1"
    -BuiltClientPath "$<TARGET_FILE:jobserver>"
    -BuiltDaemonPath "$<TARGET_FILE:jobserverd>"
    -RegisterScript "${CMAKE_CURRENT_SOURCE_DIR}/Register-Jobserver.ps1"
    -AsanEnabled "$<BOOL:${IOJ_ENABLE_ASAN}>"
  DEPENDS jobserver jobserverd
  COMMENT "Installing and registering the canonical per-user jobserver"
  USES_TERMINAL
  VERBATIM
)
