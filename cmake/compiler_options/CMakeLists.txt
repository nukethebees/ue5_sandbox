add_library(ioj_cxx_standard INTERFACE)
add_library(ioj::cxx_standard ALIAS ioj_cxx_standard)
target_compile_features(ioj_cxx_standard INTERFACE cxx_std_23)

add_library(ioj_arch_avx2 INTERFACE)
add_library(ioj::arch_avx2 ALIAS ioj_arch_avx2)

add_library(ioj_release_optimization INTERFACE)
add_library(ioj::release_optimization ALIAS ioj_release_optimization)

set(ioj_optimized_unreal_configuration OFF)
if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
  set(ioj_optimized_unreal_configuration ON)
endif()
set(ioj_release_configuration
  "$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<BOOL:${ioj_optimized_unreal_configuration}>>"
)

add_library(ioj_unreal_avx2 INTERFACE)
add_library(ioj::unreal_avx2 ALIAS ioj_unreal_avx2)
target_link_libraries(ioj_unreal_avx2 INTERFACE
  ioj::arch_avx2
  ioj::release_optimization
)

add_library(ioj_arch_avx512 INTERFACE)
add_library(ioj::arch_avx512 ALIAS ioj_arch_avx512)

add_library(ioj_fp_precise INTERFACE)
add_library(ioj::fp_precise ALIAS ioj_fp_precise)

add_library(ioj_fp_strict INTERFACE)
add_library(ioj::fp_strict ALIAS ioj_fp_strict)

add_library(ioj_fp_fast INTERFACE)
add_library(ioj::fp_fast ALIAS ioj_fp_fast)

add_library(ioj_debug_symbols INTERFACE)
add_library(ioj::debug_symbols ALIAS ioj_debug_symbols)

if(IOJ_MSVC_FRONTEND)
  target_compile_options(ioj_release_optimization INTERFACE
    "$<${ioj_release_configuration}:/O2>"
    "$<${ioj_release_configuration}:/Ot>"
    "$<${ioj_release_configuration}:/Ob3>"
  )

  target_compile_options(ioj_arch_avx2 INTERFACE /arch:AVX2)
  target_compile_options(ioj_arch_avx512 INTERFACE /arch:AVX512)

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(ioj_fp_precise INTERFACE /clang:-ffp-contract=off)
  else()
    target_compile_options(ioj_fp_precise INTERFACE /fp:precise)
  endif()
  target_compile_options(ioj_fp_strict INTERFACE /fp:strict)
  target_compile_options(ioj_fp_fast INTERFACE /fp:fast)

  target_compile_options(ioj_debug_symbols INTERFACE /Zi)
  target_link_options(ioj_debug_symbols INTERFACE /DEBUG)
else()
  target_compile_options(ioj_release_optimization INTERFACE
    "$<${ioj_release_configuration}:-O3>"
  )

  target_compile_options(ioj_arch_avx2 INTERFACE -mavx2)
  target_compile_options(ioj_arch_avx512 INTERFACE
    -mavx2
    -mavx512f
    -mavx512cd
    -mavx512bw
    -mavx512dq
    -mavx512vl
  )

  target_compile_options(ioj_fp_precise INTERFACE -ffp-contract=off)
  target_compile_options(ioj_fp_strict INTERFACE -ffp-contract=off)
  target_compile_options(ioj_fp_fast INTERFACE
    -fno-signed-zeros
    -fno-trapping-math
    -fassociative-math
    -ffp-contract=off
  )

  target_compile_options(ioj_debug_symbols INTERFACE -g)
  if(WIN32)
    target_compile_options(ioj_debug_symbols INTERFACE -gcodeview)
    target_link_options(ioj_debug_symbols INTERFACE -Xlinker /DEBUG)
  endif()
endif()
