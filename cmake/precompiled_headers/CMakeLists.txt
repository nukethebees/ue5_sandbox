add_library(ioj_core_pch INTERFACE)
add_library(ioj::core_pch ALIAS ioj_core_pch)
target_precompile_headers(ioj_core_pch INTERFACE
  <algorithm>
  <array>
  <cassert>
  <cmath>
  <cstddef>
  <cstdint>
  <expected>
  <format>
  <limits>
  <memory>
  <optional>
  <span>
  <string>
  <string_view>
  <type_traits>
  <utility>
  <vector>
)

add_library(ioj_codegen_pch INTERFACE)
add_library(ioj::codegen_pch ALIAS ioj_codegen_pch)
target_precompile_headers(ioj_codegen_pch INTERFACE
  <filesystem>
  <fstream>
  <map>
  <ranges>
  <set>
  <sstream>
  <stdexcept>
)
target_link_libraries(ioj_codegen_pch
  INTERFACE
    ioj::core_pch
)

add_library(ioj_test_pch INTERFACE)
add_library(ioj::test_pch ALIAS ioj_test_pch)
target_precompile_headers(ioj_test_pch INTERFACE
  <gtest/gtest.h>
)
target_link_libraries(ioj_test_pch
  INTERFACE
    ioj::core_pch
)