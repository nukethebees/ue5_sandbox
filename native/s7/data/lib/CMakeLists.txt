set(s7_output_dir "${IOJ_NATIVE_OUTPUT_ROOT}/S7/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}")
add_library(native-s7-data STATIC)
target_sources(native-s7-data
  PRIVATE
    src/ast.cpp
    src/sexpression_emitter.cpp
)
target_include_directories(native-s7-data
  PUBLIC
    include
)
target_link_libraries(native-s7-data
  PUBLIC
    native-core-headers
    sandbox::native_defaults
)
ioj_enable_warnings(native-s7-data)
ioj_apply_native_abi(native-s7-data)
if(IOJ_WITH_UNREAL)
  set_target_properties(native-s7-data PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${s7_output_dir}/${UE_CONFIGURATION}"
  )
endif()
