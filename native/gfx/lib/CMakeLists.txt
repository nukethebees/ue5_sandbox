add_library(native-gfx INTERFACE)
add_library(ioj::gfx ALIAS native-gfx)
target_sources(native-gfx
  INTERFACE
    include/ioj/gfx/space_dust_math.h
)
target_include_directories(native-gfx
  INTERFACE
    include
)
target_link_libraries(native-gfx
  INTERFACE
    native-levels
    sandbox::native_defaults
)
