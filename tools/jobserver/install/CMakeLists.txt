if(NOT TARGET jobserverd)
  return()
endif()

install(TARGETS jobserverd
  RUNTIME DESTINATION bin
  COMPONENT jobserver
)

foreach(target IN ITEMS install-jobserver install-jobserver-force)
  set(force_argument)
  if(target STREQUAL "install-jobserver-force")
    set(force_argument -Force)
  endif()
  add_custom_target(${target}
    COMMAND pwsh -NoProfile -File
      "${CMAKE_CURRENT_SOURCE_DIR}/Install-Jobserver.ps1"
      -BuiltDaemonPath "$<TARGET_FILE:jobserverd>"
      -RegisterScript "${CMAKE_CURRENT_SOURCE_DIR}/Register-Jobserver.ps1"
      -AsanEnabled "$<BOOL:${IOJ_ENABLE_ASAN}>"
      ${force_argument}
    DEPENDS jobserverd
    COMMENT "Installing and registering the canonical per-user jobserver"
    USES_TERMINAL
    VERBATIM
  )
endforeach()
