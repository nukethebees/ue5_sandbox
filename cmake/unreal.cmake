function(add_unreal_target target_name unreal_target)
  add_custom_target(${target_name}
    COMMAND "${UE_BUILD_SCRIPT}" ${unreal_target} ${UE_PLATFORM} ${UE_CONFIGURATION}
      "-Project=${SANDBOX_UPROJECT}" -WaitMutex
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building ${unreal_target} ${UE_PLATFORM} ${UE_CONFIGURATION} through UnrealBuildTool"
    USES_TERMINAL
    VERBATIM
  )
endfunction()
