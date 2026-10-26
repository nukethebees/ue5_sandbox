function(ioj_stage_runtime_dependencies target)
  if(WIN32)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}"
        "-Druntime_dlls=$<TARGET_RUNTIME_DLLS:${target}>"
        "-Ddestination=$<TARGET_FILE_DIR:${target}>"
        -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/stage_runtime_dependencies.cmake"
      VERBATIM
    )
  endif()

  if(IOJ_WITH_UNREAL AND IOJ_WITH_TRACY)
    target_sources(${target}
      PRIVATE
        "${PROJECT_SOURCE_DIR}/native/profiling/src/executable_lifetime.cpp"
    )
    target_link_libraries(${target}
      PRIVATE
        Tracy::TracyClient
    )
  endif()
endfunction()
