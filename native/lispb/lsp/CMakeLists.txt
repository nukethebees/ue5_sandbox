add_executable(lispb-lsp EXCLUDE_FROM_ALL)
target_sources(lispb-lsp
  PRIVATE
    main.cpp
    analysis.cpp
    highlighting.cpp
    protocol.cpp
)
target_link_libraries(lispb-lsp
  PRIVATE
    lispb-target-compiler
    nlohmann_json::nlohmann_json
    sandbox::native_defaults
)
ioj_enable_warnings(lispb-lsp CODEGEN)

add_executable(lispb-protocol-tests EXCLUDE_FROM_ALL)
target_sources(lispb-protocol-tests
  PRIVATE
    tests/protocol_tests.cpp
    protocol.cpp
)
target_link_libraries(lispb-protocol-tests
  PRIVATE
    lispb-syntax
    nlohmann_json::nlohmann_json
    GTest::gtest_main
    sandbox::native_defaults
)
add_test(NAME LispbFileUris COMMAND lispb-protocol-tests)
set_tests_properties(LispbFileUris PROPERTIES LABELS "tool;lispb-lsp")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME LispbLanguageServer
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/session.py" "$<TARGET_FILE:lispb-lsp>"
)
set_tests_properties(LispbLanguageServer PROPERTIES LABELS "tool;lispb-lsp" TIMEOUT 90)

if(WIN32)
  set(editor_dir "${CMAKE_CURRENT_SOURCE_DIR}/editors")
  set(test_dir "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  set(package_dir "${CMAKE_CURRENT_BINARY_DIR}/packages")
  set(vscode_package "${package_dir}/lispb-vscode.vsix")
  set(visualstudio_package "${package_dir}/lispb-visualstudio.vsix")
  set(visualstudio_package_args "${visual_studio_root}/MSBuild/Current/Bin/MSBuild.exe")

  set(vscode_sources
    editors/vscode/package.json
    editors/vscode/package-lock.json
    editors/vscode/extension.js
  )
  set(visualstudio_sources
    editors/visualstudio/Lispb.csproj
    editors/visualstudio/Client.cs
    editors/visualstudio/extension.vsixmanifest
  )

  foreach(editor IN ITEMS vscode visualstudio)
    set(package "${${editor}_package}")
    add_custom_command(OUTPUT "${package}"
      COMMAND "${Python3_EXECUTABLE}" "${editor_dir}/package.py"
        "${editor}" "$<TARGET_FILE:lispb-lsp>" "${package}" ${${editor}_package_args}
      DEPENDS lispb-lsp editors/package.py ${${editor}_sources}
      VERBATIM
    )
    add_custom_target(lispb-${editor} DEPENDS "${package}")
  endforeach()

  cmake_path(GET IOJ_VSWHERE_EXECUTABLE PARENT_PATH installer_dir)
  set(vsix_engine "${installer_dir}/resources/app/ServiceHub/Services/Microsoft.VisualStudio.Setup.Service/VsixInstaller/Microsoft.VisualStudio.ExtensionEngine.dll")
  add_test(NAME LispbEditorPackages
    COMMAND "${Python3_EXECUTABLE}" "${test_dir}/packages.py"
      "${package_dir}" "$<TARGET_FILE:lispb-lsp>" "${vsix_engine}"
  )
  set_tests_properties(LispbEditorPackages PROPERTIES LABELS "tool;lispb-editors" TIMEOUT 90)
  add_test(NAME LispbEditorInstall
    COMMAND "${Python3_EXECUTABLE}" "${test_dir}/test_install.py"
  )
  set_tests_properties(LispbEditorInstall PROPERTIES LABELS "tool;lispb-editors")

  add_custom_target(check-lispb-editors
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}"
      -L "^lispb-(lsp|editors)$" --no-tests=error --output-on-failure
    COMMAND "${CMAKE_COMMAND}" -E echo "VS Code package: ${vscode_package}"
    COMMAND "${CMAKE_COMMAND}" -E echo "Visual Studio 2026 package: ${visualstudio_package}"
    DEPENDS lispb-vscode lispb-visualstudio lispb-protocol-tests
    COMMENT "Testing Lispb editor packages"
    USES_TERMINAL
    VERBATIM
  )

  add_custom_target(install-lispb-editors
    COMMAND "${Python3_EXECUTABLE}" "${editor_dir}/install.py"
      "${IOJ_VSWHERE_EXECUTABLE}" "${vscode_package}" "${visualstudio_package}"
    DEPENDS check-lispb-editors
    COMMENT "Installing Lispb editor packages"
    USES_TERMINAL
    VERBATIM
  )
endif()
