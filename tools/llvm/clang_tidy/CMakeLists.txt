# Included by LLVM after clangTidy and the clang-tidy executable exist.
add_library(ioj-tidy-checks OBJECT
  lib/ForbiddenStdType.cpp
  lib/ForbiddenStdType.hpp
  lib/NoPairCheck.cpp
  lib/NoPairCheck.hpp
  lib/NoTupleCheck.cpp
  lib/NoTupleCheck.hpp
  lib/SimulationPolicy.cpp
  lib/SimulationPolicy.hpp
  lib/LoopViewConstructionCheck.cpp
  lib/LoopViewConstructionCheck.hpp
  lib/LoopViewAccessorCallCheck.cpp
  lib/LoopViewAccessorCallCheck.hpp
  lib/IojTidyModule.cpp
  lib/LoopConditionCallCheck.cpp
  lib/LoopConditionCallCheck.hpp)
target_compile_features(ioj-tidy-checks PRIVATE cxx_std_23)
llvm_update_compile_flags(ioj-tidy-checks)
target_include_directories(ioj-tidy-checks PRIVATE
  "${CLANG_SOURCE_DIR}/../clang-tools-extra")
target_link_libraries(ioj-tidy-checks PRIVATE clangTidy)
target_link_libraries(clang-tidy PRIVATE ioj-tidy-checks)
