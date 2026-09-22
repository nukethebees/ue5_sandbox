if(NOT DEFINED ToolPath OR ToolPath STREQUAL "")
  message(FATAL_ERROR "ToolPath is required.")
endif()

if(NOT EXISTS "${ToolPath}")
  message(FATAL_ERROR
    "The canonical set-live-coding-disabled executable is not installed at '${ToolPath}'. "
    "Run the install-set-live-coding-disabled target first.")
endif()
