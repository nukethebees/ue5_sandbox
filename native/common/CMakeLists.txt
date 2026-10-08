add_library(native-common STATIC)
target_sources(native-common PRIVATE
  lib/src/files.cpp)
target_include_directories(native-common PUBLIC lib/include)
target_link_libraries(native-common PUBLIC sandbox::native_defaults)
ioj_enable_warnings(native-common)
ioj_apply_native_abi(native-common)
if(IOJ_WITH_UNREAL)
  set_target_properties(native-common PROPERTIES ARCHIVE_OUTPUT_DIRECTORY
    "${IOJ_NATIVE_OUTPUT_ROOT}/Common/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}")
endif()
