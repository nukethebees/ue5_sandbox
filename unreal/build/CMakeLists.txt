set(sandbox_unreal_native_dependency_targets
  native-memory
  sbx-mimalloc
  native-simulation-unreal
  native-level-authoring
  sandbox-s7
  native-core
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
    sandbox-image
    sandbox-mesh-gen
    check-space-game-layers
    check-generated-code
    check-generated-slate-code
    compile-ui-glow-material
    compile-world-soft-target-material
    compile-migrated-materials
    sandbox-material-gen
)
set_property(TARGET unreal-generated-inputs PROPERTY FOLDER "Sandbox/Unreal/Dependencies")

add_unreal_target(editor SandboxEditor)
add_unreal_target(game Sandbox)
add_dependencies(editor unreal-native-dependencies unreal-generated-inputs)
add_dependencies(game unreal-native-dependencies check-space-game-layers check-generated-code)
set_property(TARGET editor PROPERTY FOLDER "Sandbox/Unreal/Build")
set_property(TARGET game PROPERTY FOLDER "Sandbox/Unreal/Build")

add_unreal_editor_target(run-editor
  EXECUTABLE "${UE_EDITOR_EXE}"
  ARGUMENTS "${SANDBOX_LIVE_CODING_DISABLED_ARGUMENT}"
  DEPENDS editor
  COMMENT "Launching ${SANDBOX_UPROJECT} with Live Coding disabled"
  FOLDER "Sandbox/Unreal/Editor"
)
add_unreal_editor_target(run-editor-debug
  EXECUTABLE "${UE_EDITOR_EXE}"
  ARGUMENTS "${SANDBOX_LIVE_CODING_DISABLED_ARGUMENT}" -WaitForDebugger
  DEPENDS editor
  COMMENT "Launching ${SANDBOX_UPROJECT} with Live Coding disabled and waiting for a debugger"
  FOLDER "Sandbox/Unreal/Editor"
)

add_unreal_packaging_targets()
foreach(sandbox_unreal_packaging_target IN ITEMS
    cook
    cook-incremental
    stage
    archive
    run-staged
    verify-package)
  if(TARGET ${sandbox_unreal_packaging_target})
    set_property(TARGET ${sandbox_unreal_packaging_target}
      PROPERTY FOLDER "Sandbox/Unreal/Packaging")
  endif()
endforeach()

add_custom_target(worktree-dependencies
  COMMAND "${SANDBOX_SET_LIVE_CODING_DISABLED_TOOL}"
    --settings-path
    "${PROJECT_SOURCE_DIR}/Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini"
  DEPENDS
    sandbox-set-live-coding-disabled-preflight
    unreal-native-dependencies
    sandbox-image
    sandbox-material-gen
    sandbox-mesh-gen
    check-generated-code
    check-generated-slate-code
    compile-ui-glow-material
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  COMMENT "Preparing worktree dependencies and disabling Live Coding"
  VERBATIM
)
set_property(TARGET worktree-dependencies PROPERTY FOLDER "Sandbox/Unreal/Dependencies")
