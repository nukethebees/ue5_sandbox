include_guard(GLOBAL)

function(sandbox_add_cpu_features source_directory)
  # Keep the pinned dependency's sources explicit. Its upstream CMake project uses
  # CONFIGURE_DEPENDS for this list, which adds a VerifyGlobs command to every
  # Ninja build. Update this list with the submodule when its source set changes.
  include(GNUInstallDirs)

  set(cpu_features_headers
    "${source_directory}/include/cpu_features_macros.h"
    "${source_directory}/include/cpu_features_cache_info.h"
  )
  set(cpu_features_sources
    "${source_directory}/src/impl_aarch64_cpuid.c"
    "${source_directory}/src/impl_aarch64_freebsd_or_openbsd.c"
    "${source_directory}/src/impl_aarch64_linux_or_android.c"
    "${source_directory}/src/impl_aarch64_macos_or_iphone.c"
    "${source_directory}/src/impl_aarch64_windows.c"
    "${source_directory}/src/impl_arm_linux_or_android.c"
    "${source_directory}/src/impl_loongarch_linux.c"
    "${source_directory}/src/impl_mips_linux_or_android.c"
    "${source_directory}/src/impl_ppc_linux.c"
    "${source_directory}/src/impl_riscv_linux.c"
    "${source_directory}/src/impl_s390x_linux.c"
    "${source_directory}/src/impl_x86_freebsd.c"
    "${source_directory}/src/impl_x86_linux_or_android.c"
    "${source_directory}/src/impl_x86_macos.c"
    "${source_directory}/src/impl_x86_windows.c"
  )

  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^mips")
    list(APPEND cpu_features_headers "${source_directory}/include/cpuinfo_mips.h")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "(^aarch64)|(^arm64)|(^ARM64)")
    list(APPEND cpu_features_headers "${source_directory}/include/cpuinfo_aarch64.h")
    list(APPEND cpu_features_sources
      "${source_directory}/include/internal/cpuid_aarch64.h"
      "${source_directory}/include/internal/windows_utils.h"
    )
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    list(APPEND cpu_features_headers "${source_directory}/include/cpuinfo_arm.h")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(x86_64)|(AMD64|amd64)|(^i.86$)")
    list(APPEND cpu_features_headers "${source_directory}/include/cpuinfo_x86.h")
    list(APPEND cpu_features_sources
      "${source_directory}/include/internal/cpuid_x86.h"
      "${source_directory}/include/internal/windows_utils.h"
    )
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)")
    list(APPEND cpu_features_headers "${source_directory}/include/cpuinfo_ppc.h")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(s390x)")
    list(APPEND cpu_features_headers "${source_directory}/include/cpuinfo_s390x.h")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^riscv")
    list(APPEND cpu_features_headers "${source_directory}/include/cpuinfo_riscv.h")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^loongarch")
    list(APPEND cpu_features_headers "${source_directory}/include/cpuinfo_loongarch.h")
  else()
    message(FATAL_ERROR "Unsupported architecture ${CMAKE_SYSTEM_PROCESSOR}")
  endif()

  add_library(cpu_features_utils OBJECT
    "${source_directory}/include/internal/bit_utils.h"
    "${source_directory}/include/internal/filesystem.h"
    "${source_directory}/include/internal/stack_line_reader.h"
    "${source_directory}/include/internal/string_view.h"
    "${source_directory}/src/filesystem.c"
    "${source_directory}/src/stack_line_reader.c"
    "${source_directory}/src/string_view.c"
  )
  set_property(TARGET cpu_features_utils PROPERTY C_STANDARD 99)
  target_include_directories(cpu_features_utils
    PUBLIC "${source_directory}/include"
    PRIVATE "${source_directory}/include/internal"
  )
  target_compile_definitions(cpu_features_utils PUBLIC STACK_LINE_READER_BUFFER_SIZE=1024)

  set(cpu_features_object_sources "$<TARGET_OBJECTS:cpu_features_utils>")
  if(UNIX)
    add_library(cpu_features_unix_hardware_detection OBJECT
      "${source_directory}/include/internal/hwcaps.h"
      "${source_directory}/src/hwcaps_linux_or_android.c"
      "${source_directory}/src/hwcaps_freebsd_or_openbsd.c"
      "${source_directory}/src/hwcaps.c"
    )
    set_property(TARGET cpu_features_unix_hardware_detection PROPERTY C_STANDARD 99)
    target_include_directories(cpu_features_unix_hardware_detection
      PUBLIC "${source_directory}/include"
      PRIVATE "${source_directory}/include/internal"
    )
    target_compile_definitions(cpu_features_unix_hardware_detection
      PUBLIC STACK_LINE_READER_BUFFER_SIZE=1024
    )

    include(CheckIncludeFile)
    include(CheckSymbolExists)
    check_include_file(dlfcn.h HAVE_DLFCN_H)
    if(HAVE_DLFCN_H)
      target_compile_definitions(cpu_features_unix_hardware_detection PRIVATE HAVE_DLFCN_H)
    endif()
    check_symbol_exists(getauxval "sys/auxv.h" HAVE_STRONG_GETAUXVAL)
    check_symbol_exists(elf_aux_info "sys/auxv.h" HAVE_STRONG_ELF_AUX_INFO)
    if(HAVE_STRONG_GETAUXVAL)
      target_compile_definitions(cpu_features_unix_hardware_detection PRIVATE HAVE_STRONG_GETAUXVAL)
    endif()
    if(HAVE_STRONG_ELF_AUX_INFO)
      target_compile_definitions(cpu_features_unix_hardware_detection PUBLIC HAVE_STRONG_ELF_AUX_INFO)
    endif()

    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(x86_64)|(AMD64|amd64)|(^i.86$)")
      list(APPEND cpu_features_object_sources
        "$<TARGET_OBJECTS:cpu_features_unix_hardware_detection>"
      )
    endif()
  endif()

  add_library(cpu_features ${cpu_features_headers} ${cpu_features_sources}
    ${cpu_features_object_sources})
  set_property(TARGET cpu_features PROPERTY C_STANDARD 99)
  target_include_directories(cpu_features
    PUBLIC
      "$<BUILD_INTERFACE:${source_directory}/include>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cpu_features>"
    PRIVATE "${source_directory}/include/internal"
  )
  target_compile_definitions(cpu_features PUBLIC STACK_LINE_READER_BUFFER_SIZE=1024)
  target_link_libraries(cpu_features PUBLIC ${CMAKE_DL_LIBS})
  if(APPLE)
    target_compile_definitions(cpu_features PRIVATE HAVE_SYSCTLBYNAME)
  endif()

  add_library(CpuFeatures::cpu_features ALIAS cpu_features)
endfunction()
