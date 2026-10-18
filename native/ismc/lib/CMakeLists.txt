# Isolate AVX2 from the scalar reference and preserve packing arithmetic.
add_library(native-ismc STATIC)
add_library(ioj::ismc ALIAS native-ismc)
add_library(native-ismc-highway-avx2 OBJECT)

target_sources(native-ismc
  PRIVATE
    src/packing.cpp
    src/packing_scalar.cpp
    include/ioj/ismc/packing.h
    include/ioj/ismc/quat32.h
    include/ioj/ismc/quat32_packing.h
    include/ioj/ismc/transform.h
    include/ioj/ismc/render.h
)
target_sources(native-ismc-highway-avx2
  PRIVATE
    src/packing_highway.cpp
)
set_target_properties(native-ismc native-ismc-highway-avx2 PROPERTIES UNITY_BUILD OFF)
target_include_directories(native-ismc
  PUBLIC
    include
)
target_include_directories(native-ismc-highway-avx2
  PRIVATE
    include
)
target_link_libraries(native-ismc
  PUBLIC
    native-core-headers
    sandbox::native_defaults
  PRIVATE
    hwy
    ioj::fp_precise
    native-ismc-highway-avx2
)
target_link_libraries(native-ismc-highway-avx2
  PRIVATE
    native-core-headers
)
ioj_configure_highway_target(native-ismc-highway-avx2 avx2)
ioj_apply_native_abi(native-ismc)
ioj_enable_warnings(native-ismc)
set_target_properties(native-ismc PROPERTIES OUTPUT_NAME SandboxNativeISMC)
if(IOJ_WITH_UNREAL)
  set_target_properties(native-ismc PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${IOJ_NATIVE_OUTPUT_ROOT}/ISMC/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()
