add_library(native-level-authoring STATIC)
target_sources(native-level-authoring PRIVATE
  include/sandbox/level_authoring/LevelDefinitionReader.h
  src/LevelDefinitionReader.cpp
  include/sandbox/level_authoring/CampaignDefinition.h
  include/sandbox/level_authoring/CampaignDefinitionReader.h
  src/CampaignDefinitionReader.cpp
  include/sandbox/level_authoring/LevelDefinitionWriter.h
  src/LevelDefinitionWriter.cpp
  include/sandbox/level_authoring/CatalogValidation.h
  src/CatalogValidation.cpp
)
target_compile_features(native-level-authoring PUBLIC cxx_std_23)
target_include_directories(native-level-authoring PUBLIC include)
target_link_libraries(native-level-authoring
  PUBLIC native-simulation
  PRIVATE sandbox-s7 sandbox::warnings_as_errors
)
configure_sandbox_s7_abi(native-level-authoring)

set_target_properties(native-level-authoring PROPERTIES
  OUTPUT_NAME SandboxNativeLevelAuthoring
)
if(SANDBOX_WITH_UNREAL)
  set_target_properties(native-level-authoring PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/LevelAuthoring/${SANDBOX_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()
