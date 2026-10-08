include_guard(GLOBAL)

function(ioj_add_benchmark_report target executable filter output repetitions minimum_time description)
  cmake_path(GET output PARENT_PATH output_directory)
  add_custom_target(${target}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_directory}"
    COMMAND "$<TARGET_FILE:${executable}>"
      "--benchmark_filter=${filter}"
      "--benchmark_repetitions=${repetitions}"
      "--benchmark_min_time=${minimum_time}"
      --benchmark_enable_random_interleaving=true
      --benchmark_display_aggregates_only=true
      "--benchmark_out=${output}"
      --benchmark_out_format=json
    DEPENDS ${executable}
    COMMENT "${description}"
    USES_TERMINAL
    VERBATIM
  )
endfunction()
