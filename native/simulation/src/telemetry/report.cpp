#include <ioj/sim/telemetry/report.h>

#include <utility>

namespace ioj::sim::telemetry {
Report::Report(RunRecord record)
    : run{std::move(record)} {}
}
