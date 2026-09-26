if(NOT LLVM_ROOT)
  message(FATAL_ERROR "Custom clang-tidy checks require LLVM_ROOT with LLVM development packages.")
endif()

set(LLVM_DIR "${LLVM_ROOT}/lib/cmake/llvm")
set(Clang_DIR "${LLVM_ROOT}/lib/cmake/clang")
find_package(LLVM REQUIRED CONFIG NO_DEFAULT_PATH PATHS "${LLVM_DIR}")
find_package(Clang REQUIRED CONFIG NO_DEFAULT_PATH PATHS "${Clang_DIR}")

if(NOT LLVM_EXPORT_SYMBOLS_FOR_PLUGINS)
  message(FATAL_ERROR "LLVM_ROOT must provide clang-tidy built with LLVM_EXPORT_SYMBOLS_FOR_PLUGINS=ON.")
endif()

# Match the host executable's configuration: unoptimized inline LLVM functions
# can reference symbols absent from a Release clang-tidy executable's exports.
string(TOUPPER "${LLVM_BUILD_TYPE}" llvm_configuration)
string(TOUPPER "${CMAKE_BUILD_TYPE}" plugin_configuration)
set(CMAKE_CXX_FLAGS_${plugin_configuration} "${CMAKE_CXX_FLAGS_${llvm_configuration}}")

add_library(ioj-tidy-module MODULE
  IojTidyModule.cpp
  LoopConditionCallCheck.cpp
  LoopConditionCallCheck.hpp)
target_compile_features(ioj-tidy-module PRIVATE cxx_std_23)
target_include_directories(ioj-tidy-module SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(ioj-tidy-module PRIVATE LLVM_BUILD_STATIC CLANG_BUILD_STATIC _ITERATOR_DEBUG_LEVEL=0)
set_target_properties(ioj-tidy-module PROPERTIES
  MSVC_RUNTIME_LIBRARY MultiThreadedDLL
  MSVC_RUNTIME_CHECKS "")
if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(ioj-tidy-module PRIVATE /GR-)
endif()
if(LLVM_ENABLE_ASSERTIONS)
  target_compile_options(ioj-tidy-module PRIVATE /UNDEBUG)
endif()
# Link the executable's import library so registration uses the host registry.
target_link_libraries(ioj-tidy-module PRIVATE clang-tidy)

add_test(NAME ClangTidy.LoopConditionCall
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_loop_condition_call.py"
    --clang-tidy "${IOJ_CLANG_TIDY_EXECUTABLE}"
    --plugin "$<TARGET_FILE:ioj-tidy-module>"
    --source-dir "${PROJECT_SOURCE_DIR}")
set_tests_properties(ClangTidy.LoopConditionCall PROPERTIES LABELS "clang-tidy")
