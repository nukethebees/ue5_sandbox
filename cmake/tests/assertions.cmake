function(sandbox_assert_equal test_name actual expected)
  if(NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "${test_name} failed: expected '${expected}', got '${actual}'.")
  endif()
endfunction()

function(sandbox_assert_list_equal test_name actual expected)
  list(JOIN actual "|" actual_text)
  list(JOIN expected "|" expected_text)
  sandbox_assert_equal("${test_name}" "${actual_text}" "${expected_text}")
endfunction()
