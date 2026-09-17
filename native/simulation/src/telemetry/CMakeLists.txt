target_sources(native-simulation PRIVATE
  ../../include/ioj/sim/telemetry/level_telemetry_block_history.h
  ../../include/ioj/sim/telemetry/level_telemetry_analysis.h
  ../../include/ioj/sim/telemetry/level_telemetry_history.h
  ../../include/ioj/sim/telemetry/level_telemetry_json_validation.h
  ../../include/ioj/sim/telemetry/level_telemetry_run_end_reason.h
  ../../include/ioj/sim/telemetry/level_telemetry_run_record.h
  level_telemetry_block_history.cpp
  level_telemetry_analysis.cpp
  level_telemetry_json_validation.cpp
)
