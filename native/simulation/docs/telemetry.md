# Level telemetry

Telemetry records simulation ticks, population and combat history for the post-run dashboard.
Use Tracy for CPU timings, allocations, hardware context and performance comparisons.
Navigation counters remain available to simulation tests and benchmarks for behavioural checks.

JSON reports accept only schema 11. Older reports are unsupported; there is no migration path.
Schema 11 removes hardware/build environment metadata and the duplicate presentation-mode
string. Battle samples require both laser counters. The reader validates values as it constructs
the report, without walking the finished report again. Serialization validates its mutable input.
An exported report is not a mission progress save.
