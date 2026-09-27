include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/jobserver_integration.cmake")

add_custom_target(csharp-host-tools)

function(sandbox_dotnet_configuration output_variable)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(${output_variable} Debug PARENT_SCOPE)
  else()
    set(${output_variable} Release PARENT_SCOPE)
  endif()
endfunction()

function(sandbox_find_dotnet)
  find_program(IOJ_DOTNET_EXECUTABLE NAMES dotnet REQUIRED)
endfunction()

function(sandbox_add_dotnet_host_tool target_name output_variable project_file)
  cmake_path(ABSOLUTE_PATH project_file
    BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    NORMALIZE
    OUTPUT_VARIABLE project_file
  )
  get_filename_component(tool_name "${project_file}" NAME_WE)

  sandbox_dotnet_configuration(dotnet_configuration)
  sandbox_find_dotnet()

  set(output_directory
    "${CMAKE_BINARY_DIR}/host-tools/${tool_name}/${dotnet_configuration}"
  )
  set(output_file "${output_directory}/${tool_name}.exe")
  set(intermediate_directory "${output_directory}/obj/")

  sandbox_jobserver_command(build_command STANDARD build "Build .NET host tool ${tool_name}")
  # MSBuild owns transitive input tracking; run it even when the apphost exists.
  add_custom_target(${target_name}
    COMMAND ${build_command} "${IOJ_DOTNET_EXECUTABLE}" build "${project_file}"
      --configuration "${dotnet_configuration}"
      --output "${output_directory}"
      --nologo
      "-p:IsStandaloneTool=false"
      "-p:SandboxCMakeHostToolBuild=true"
      "-p:BaseIntermediateOutputPath=${intermediate_directory}"
      "-p:MSBuildProjectExtensionsPath=${intermediate_directory}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    BYPRODUCTS "${output_file}"
      "${output_directory}/${tool_name}.dll"
      "${output_directory}/${tool_name}.deps.json"
      "${output_directory}/${tool_name}.runtimeconfig.json"
    COMMENT "Building .NET host tool ${tool_name}"
    VERBATIM
  )
  add_dependencies(csharp-host-tools ${target_name})
  add_executable(${target_name}-executable IMPORTED GLOBAL)
  set_property(TARGET ${target_name}-executable PROPERTY IMPORTED_LOCATION "${output_file}")
  add_dependencies(${target_name}-executable ${target_name})
  set(${output_variable} "$<TARGET_FILE:${target_name}-executable>" PARENT_SCOPE)
endfunction()
