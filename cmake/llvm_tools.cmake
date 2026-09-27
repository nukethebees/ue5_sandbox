include_guard(GLOBAL)

set(LLVM_ROOT "$ENV{LLVM_ROOT}" CACHE PATH "LLVM toolchain installation root")

function(ioj_find_llvm_tool output)
  if(LLVM_ROOT)
    find_program(llvm_tool NAMES ${ARGN}
      PATHS "${LLVM_ROOT}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)
  else()
    find_program(llvm_tool NAMES ${ARGN} NO_CACHE REQUIRED)
  endif()
  set(${output} "${llvm_tool}" CACHE INTERNAL "LLVM tool derived from LLVM_ROOT" FORCE)
  set(${output} "${llvm_tool}" PARENT_SCOPE)
endfunction()
