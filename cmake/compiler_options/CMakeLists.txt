add_library(sandbox_arch_avx2 INTERFACE)
add_library(sandbox::arch_avx2 ALIAS sandbox_arch_avx2)

add_library(sandbox_release_optimization INTERFACE)
add_library(sandbox::release_optimization ALIAS sandbox_release_optimization)

set(sandbox_optimized_unreal_configuration OFF)
if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
  set(sandbox_optimized_unreal_configuration ON)
endif()
set(sandbox_release_configuration
  "$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<BOOL:${sandbox_optimized_unreal_configuration}>>"
)

add_library(sandbox_unreal_avx2 INTERFACE)
add_library(sandbox::unreal_avx2 ALIAS sandbox_unreal_avx2)
target_link_libraries(sandbox_unreal_avx2 INTERFACE
  sandbox::arch_avx2
  sandbox::release_optimization
)

add_library(sandbox_arch_avx512 INTERFACE)
add_library(sandbox::arch_avx512 ALIAS sandbox_arch_avx512)

add_library(sandbox_fp_precise INTERFACE)
add_library(sandbox::fp_precise ALIAS sandbox_fp_precise)

add_library(sandbox_fp_strict INTERFACE)
add_library(sandbox::fp_strict ALIAS sandbox_fp_strict)

add_library(sandbox_fp_fast INTERFACE)
add_library(sandbox::fp_fast ALIAS sandbox_fp_fast)

add_library(sandbox_debug_symbols INTERFACE)
add_library(sandbox::debug_symbols ALIAS sandbox_debug_symbols)

if(SANDBOX_MSVC_FRONTEND)
  target_compile_options(sandbox_release_optimization INTERFACE
    "$<${sandbox_release_configuration}:/O2>"
    "$<${sandbox_release_configuration}:/Ot>"
    "$<${sandbox_release_configuration}:/Ob3>"
  )

  target_compile_options(sandbox_arch_avx2 INTERFACE /arch:AVX2)
  target_compile_options(sandbox_arch_avx512 INTERFACE /arch:AVX512)

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(sandbox_fp_precise INTERFACE /clang:-ffp-contract=off)
  else()
    target_compile_options(sandbox_fp_precise INTERFACE /fp:precise)
  endif()
  target_compile_options(sandbox_fp_strict INTERFACE /fp:strict)
  target_compile_options(sandbox_fp_fast INTERFACE /fp:fast)

  target_compile_options(sandbox_debug_symbols INTERFACE /Zi)
  target_link_options(sandbox_debug_symbols INTERFACE /DEBUG)
else()
  target_compile_options(sandbox_release_optimization INTERFACE
    "$<${sandbox_release_configuration}:-O3>"
  )

  target_compile_options(sandbox_arch_avx2 INTERFACE -mavx2)
  target_compile_options(sandbox_arch_avx512 INTERFACE
    -mavx2
    -mavx512f
    -mavx512cd
    -mavx512bw
    -mavx512dq
    -mavx512vl
  )

  target_compile_options(sandbox_fp_precise INTERFACE -ffp-contract=off)
  target_compile_options(sandbox_fp_strict INTERFACE -ffp-contract=off)
  target_compile_options(sandbox_fp_fast INTERFACE
    -fno-signed-zeros
    -fno-trapping-math
    -fassociative-math
    -ffp-contract=off
  )

  target_compile_options(sandbox_debug_symbols INTERFACE -g)
  if(WIN32)
    target_compile_options(sandbox_debug_symbols INTERFACE -gcodeview)
    target_link_options(sandbox_debug_symbols INTERFACE -Xlinker /DEBUG)
  endif()
endif()
