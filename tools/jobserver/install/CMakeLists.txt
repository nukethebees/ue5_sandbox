if(NOT TARGET jobserverd)
  return()
endif()

install(TARGETS jobserverd
  RUNTIME DESTINATION bin
  COMPONENT jobserver
)

add_custom_target(install-jobserver
  COMMAND pwsh -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/Install-Jobserver.ps1"
    -BuiltDaemonPath "$<TARGET_FILE:jobserverd>"
    -RegisterScript "${CMAKE_CURRENT_SOURCE_DIR}/Register-Jobserver.ps1"
    -AsanEnabled "$<BOOL:${IOJ_ENABLE_ASAN}>"
  DEPENDS jobserverd
  COMMENT "Installing and registering the canonical per-user jobserver"
  USES_TERMINAL
  VERBATIM
)
