include_guard(GLOBAL)

function(sandbox_add_dotnet_host_tool target_name output_variable project_file)
  cmake_path(ABSOLUTE_PATH project_file
    BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    NORMALIZE
    OUTPUT_VARIABLE project_file
  )
  get_filename_component(tool_name "${project_file}" NAME_WE)

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(dotnet_configuration Debug)
  else()
    set(dotnet_configuration Release)
  endif()

  find_program(SANDBOX_DOTNET_EXECUTABLE NAMES dotnet REQUIRED)

  set(output_directory
    "${CMAKE_BINARY_DIR}/host-tools/${tool_name}/${dotnet_configuration}"
  )
  set(output_file "${output_directory}/${tool_name}.exe")
  set(intermediate_directory "${output_directory}/obj/")

  add_custom_target(${target_name}
    COMMAND "${SANDBOX_DOTNET_EXECUTABLE}" build "${project_file}"
      --configuration "${dotnet_configuration}"
      --output "${output_directory}"
      --nologo
      "-p:IsStandaloneTool=false"
      "-p:SandboxCMakeHostToolBuild=true"
      "-p:BaseIntermediateOutputPath=${intermediate_directory}"
      "-p:MSBuildProjectExtensionsPath=${intermediate_directory}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Building .NET host tool ${tool_name}"
    VERBATIM
  )
  if(TARGET csharp-host-tools)
    add_dependencies(csharp-host-tools ${target_name})
  endif()

  set(${output_variable} "${output_file}" PARENT_SCOPE)
endfunction()
