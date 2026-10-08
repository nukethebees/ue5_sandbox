add_library(native-levels STATIC)
target_sources(native-levels
  PRIVATE
    lib/src/level_definition.cpp
    lib/src/entity_validation.cpp
    lib/src/mission_validation.cpp
    lib/src/camera_validation.cpp
    lib/src/collision_grid_validation.cpp
    lib/src/campaign_definition.cpp
    lib/src/diagnostics.cpp
    lib/src/catalog_validation.cpp
)
target_include_directories(native-levels
  PUBLIC
    lib/include
)
target_link_libraries(native-levels
  PUBLIC
    native-core-headers
    sandbox::native_defaults
  PRIVATE
    native-core
)
ioj_enable_warnings(native-levels)
ioj_apply_native_abi(native-levels)
if(IOJ_WITH_UNREAL)
  set(levels_output_dir "${IOJ_NATIVE_OUTPUT_ROOT}/Levels/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}")
  set_target_properties(native-levels PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${levels_output_dir}/${UE_CONFIGURATION}"
  )
endif()
