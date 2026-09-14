#pragma once

#include <string>
#include <vector>

namespace jobserver::cli {
enum class DoctorStatus {
    pass,
    warning,
    failure,
};

struct DoctorCheck {
    DoctorStatus status{};
    std::string name;
    std::string detail;
};

[[nodiscard]] auto run_doctor() -> std::vector<DoctorCheck>;
}
