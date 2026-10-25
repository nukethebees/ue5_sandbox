add_library(native-presentation STATIC)
target_sources(native-presentation
  PRIVATE
    src/boom_audio_library.cpp
    src/radar.cpp
    src/soft_target.cpp
)
target_include_directories(native-presentation
  PUBLIC
    include
)
target_link_libraries(native-presentation
  PUBLIC
    native-simulation
    native-ui
)
ioj_enable_warnings(native-presentation)
ioj_apply_native_abi(native-presentation)
if(IOJ_WITH_UNREAL)
  set_target_properties(native-presentation PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${IOJ_NATIVE_OUTPUT_ROOT}/Presentation/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()
