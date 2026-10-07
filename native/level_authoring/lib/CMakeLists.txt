add_library(native-level-authoring STATIC)
target_sources(native-level-authoring PRIVATE
  src/reader_utilities.h
  include/sandbox/level_authoring/LevelDefinitionReader.h
  src/LevelDefinitionReader.cpp
  include/sandbox/level_authoring/CampaignDefinitionReader.h
  src/CampaignDefinitionReader.cpp
  include/sandbox/level_authoring/LevelDefinitionWriter.h
  src/LevelDefinitionWriter.cpp
  include/sandbox/level_authoring/CatalogValidation.h
  src/CatalogValidation.cpp
)
target_include_directories(native-level-authoring PUBLIC include)
target_link_libraries(native-level-authoring
  PUBLIC
    sandbox::native_defaults
    native-levels
  PRIVATE
    sandbox-s7
)
ioj_enable_warnings(native-level-authoring)
ioj_apply_native_abi(native-level-authoring)

set_target_properties(native-level-authoring PROPERTIES
  OUTPUT_NAME SandboxNativeLevelAuthoring
)
if(IOJ_WITH_UNREAL)
  set_target_properties(native-level-authoring PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/LevelAuthoring/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()
