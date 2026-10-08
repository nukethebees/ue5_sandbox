set(BUILD_SHARED_LIBS ON)
set(TBB_TEST OFF CACHE BOOL "" FORCE)
set(TBB_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TBBMALLOC_BUILD OFF CACHE BOOL "" FORCE)
set(TBB_INSTALL OFF CACHE BOOL "" FORCE)
set(TBB_ENABLE_IPO OFF CACHE BOOL "" FORCE)
set(TBB_DISABLE_HWLOC_AUTOMATIC_SEARCH ON CACHE BOOL "" FORCE)
set(TBB_VERIFY_DEPENDENCY_SIGNATURE OFF CACHE BOOL "" FORCE)
# Use CMake's warning policy so global opt-outs also apply to oneTBB.
set(TBB_STRICT OFF CACHE BOOL "" FORCE)
add_subdirectory(../oneTBB oneTBB SYSTEM EXCLUDE_FROM_ALL)
ioj_apply_native_abi(tbb)
if(NOT DEFINED CMAKE_COMPILE_WARNING_AS_ERROR)
  set_property(TARGET tbb PROPERTY COMPILE_WARNING_AS_ERROR ON)
endif()
# Select the import library through CMake even when Debug uses the release CRT.
target_compile_definitions(tbb
  INTERFACE
    __TBB_NO_IMPLICIT_LINKAGE=1
)
set_target_properties(tbb PROPERTIES UNITY_BUILD OFF)

if(WIN32)
  # Keep the project runtime distinct from Unreal's bundled oneTBB runtime.
  set_target_properties(tbb PROPERTIES OUTPUT_NAME "SandboxTBB12")

  # Give the resource compiler the SDK paths when building from a plain shell.
  target_include_directories(tbb
    PRIVATE
      "$<$<COMPILE_LANGUAGE:RC>:${CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES}>"
  )
endif()

if(IOJ_WITH_UNREAL)
  set_target_properties(tbb PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${IOJ_NATIVE_OUTPUT_ROOT}/OneTBB/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
    RUNTIME_OUTPUT_DIRECTORY
      "${IOJ_NATIVE_OUTPUT_ROOT}/OneTBB/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

function(ioj_stage_tbb_runtime target)
  if(WIN32)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:TBB::tbb>" "$<TARGET_FILE_DIR:${target}>"
      VERBATIM
    )
  endif()
endfunction()
