target_sources(native-simulation PRIVATE
  ../../include/ioj/sim/telemetry/block_history.h
  ../../include/ioj/sim/telemetry/analysis.h
  ../../include/ioj/sim/telemetry/history.h
  ../../include/ioj/sim/telemetry/json_validation.h
  ../../include/ioj/sim/telemetry/run_end_reason.h
  ../../include/ioj/sim/telemetry/run_record.h
  block_history.cpp
  analysis.cpp
  ../../include/ioj/sim/telemetry/report.h
  ../../include/ioj/sim/telemetry/report_json.h
  report.cpp
  report_json.cpp
  json_validation.cpp
)
