#include <ioj/structural_diff.h>

#include <format>
#include <iterator>
#include <string>

namespace ioj {
auto DiffReport::message() const -> std::string {
    std::string text;
    for (auto const& difference : differences) {
        std::format_to(std::back_inserter(text),
                       "{}\n  Expected: {}\n  Actual:   {}\n",
                       difference.path,
                       difference.expected,
                       difference.actual);
    }
    if (omitted != 0) {
        std::format_to(std::back_inserter(text), "{} further differences omitted\n", omitted);
    }
    return text;
}
}
