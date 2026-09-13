function(sandbox_target_precompile_common_headers)
  foreach(target IN LISTS ARGN)
    if(TARGET ${target})
      target_precompile_headers(${target} PRIVATE
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
    endif()
  endforeach()
endfunction()

function(sandbox_target_precompile_codegen_headers)
  sandbox_target_precompile_common_headers(${ARGN})

  foreach(target IN LISTS ARGN)
    if(TARGET ${target})
      target_precompile_headers(${target} PRIVATE
        <filesystem>
        <fstream>
        <map>
        <ranges>
        <set>
        <sstream>
        <stdexcept>
      )
    endif()
  endforeach()
endfunction()

function(sandbox_target_precompile_test_headers)
  sandbox_target_precompile_common_headers(${ARGN})

  foreach(target IN LISTS ARGN)
    if(TARGET ${target})
      target_precompile_headers(${target} PRIVATE <gtest/gtest.h>)
    endif()
  endforeach()
endfunction()
