add_library(sandbox-handmade-math INTERFACE)
add_library(sandbox::handmade_math ALIAS sandbox-handmade-math)

target_include_directories(sandbox-handmade-math SYSTEM INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
