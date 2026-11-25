set(sandbox_unreal_native_dependency_targets
  sbx-mimalloc
  native-simulation
  native-level-authoring-s7
  sandbox-s7
  native-core
  native-ismc
  native-ui
  native-presentation
  native-profiles
  cpu_features
)
set(sandbox_unreal_native_dependencies)
foreach(sandbox_unreal_dependency IN LISTS sandbox_unreal_native_dependency_targets)
  if(TARGET ${sandbox_unreal_dependency})
    list(APPEND sandbox_unreal_native_dependencies ${sandbox_unreal_dependency})
  endif()
endforeach()
add_custom_target(unreal-native-dependencies
  DEPENDS ${sandbox_unreal_native_dependencies}
)
set_property(TARGET unreal-native-dependencies PROPERTY FOLDER "Sandbox/Unreal/Dependencies")

add_custom_target(unreal-generated-inputs
  DEPENDS
    sandbox-mesh-gen
    check-generated-code
    check-generated-slate-code
    compile-ui-glow-material
    compile-world-soft-target-material
    compile-migrated-materials
    sandbox-material-gen
)
set_property(TARGET unreal-generated-inputs PROPERTY FOLDER "Sandbox/Unreal/Dependencies")

add_unreal_target(editor SandboxEditor)
if(UE_CONFIGURATION STREQUAL "Development")
  set(game_binary_name "Sandbox")
else()
  set(game_binary_name "Sandbox-Win64-${UE_CONFIGURATION}")
endif()
set(game_import_library "${PROJECT_SOURCE_DIR}/Binaries/Win64/${game_binary_name}.lib")
set(IOJ_GAME_DLL_NAME "${game_binary_name}.dll")
add_unreal_target(game Sandbox BYPRODUCTS "${game_import_library}")
add_dependencies(editor unreal-native-dependencies unreal-generated-inputs)
add_dependencies(game unreal-native-dependencies check-generated-code)
set_property(TARGET editor PROPERTY FOLDER "Sandbox/Unreal/Build")
set_property(TARGET game PROPERTY FOLDER "Sandbox/Unreal/Build")

add_subdirectory(host)
add_unreal_packaging_targets()
foreach(sandbox_unreal_packaging_target IN ITEMS
    cook
    cook-incremental
    stage
    archive
    verify-package)
  if(TARGET ${sandbox_unreal_packaging_target})
    set_property(TARGET ${sandbox_unreal_packaging_target}
      PROPERTY FOLDER "Sandbox/Unreal/Packaging")
  endif()
endforeach()
