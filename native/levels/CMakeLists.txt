add_library(native-levels STATIC lib/src/level_definition.cpp lib/src/campaign_definition.cpp lib/src/diagnostics.cpp lib/src/catalog_validation.cpp)
target_include_directories(native-levels PUBLIC lib/include)
target_link_libraries(native-levels PUBLIC native-common native-enum-traits sandbox::native_defaults)
ioj_enable_warnings(native-levels)
ioj_apply_native_abi(native-levels)
if(IOJ_WITH_UNREAL)
  set_target_properties(native-levels PROPERTIES ARCHIVE_OUTPUT_DIRECTORY
    "${PROJECT_SOURCE_DIR}/Binaries/Native/Levels/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}")
endif()
