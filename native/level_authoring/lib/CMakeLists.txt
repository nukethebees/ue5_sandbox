add_library(native-level-authoring STATIC)
target_sources(native-level-authoring PRIVATE
  src/ast_parser.cpp
  src/level_definition_reader.cpp
  src/campaign_definition_reader.cpp
  src/level_definition_writer.cpp)
target_include_directories(native-level-authoring PUBLIC include)
target_link_libraries(native-level-authoring PUBLIC native-levels native-s7-data sandbox::native_defaults)
ioj_enable_warnings(native-level-authoring)
ioj_apply_native_abi(native-level-authoring)
set_target_properties(native-level-authoring PROPERTIES OUTPUT_NAME SandboxNativeLevelAuthoring)

add_library(native-level-authoring-s7 STATIC)
target_sources(native-level-authoring-s7 PRIVATE
  src/script_reader.cpp)
target_include_directories(native-level-authoring-s7 PUBLIC include)
target_link_libraries(native-level-authoring-s7 PUBLIC native-level-authoring PRIVATE sandbox-s7)
ioj_enable_warnings(native-level-authoring-s7)
ioj_apply_native_abi(native-level-authoring-s7)
if(IOJ_WITH_UNREAL)
  set_target_properties(native-level-authoring native-level-authoring-s7 PROPERTIES ARCHIVE_OUTPUT_DIRECTORY
    "${IOJ_NATIVE_OUTPUT_ROOT}/LevelAuthoring/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}")
endif()
