#pragma once

#include <format>
#include <utility>

namespace ioj {
template <typename OutputIterator>
class FormatWriter {
  public:
    explicit FormatWriter(OutputIterator output)
        : output_{std::move(output)} {}

    template <typename... Args>
    void operator()(std::format_string<Args...> const format, Args&&... args) {
        output_ = std::format_to(output_, format, std::forward<Args>(args)...);
    }
  private:
    OutputIterator output_;
};
}
