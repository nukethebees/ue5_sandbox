#pragma once

#include <ioj/s7/reader.h>

namespace ioj::s7 {
class FieldReader {
  public:
    explicit FieldReader(DecodeContext context)
        : context_{context} {}

    template <typename T, typename... Checks>
    void operator()(T& destination, Checks const&... checks) const {
        read_checked(destination, [&] { decode_field(context_, destination); }, checks...);
    }

    template <typename T, typename... Checks>
    void construct(std::optional<T>& destination, Checks const&... checks) const {
        read_checked(destination, [&] { read_field(context_, destination); }, checks...);
    }
  private:
    template <typename T, typename Read, typename... Checks>
    void read_checked(T& destination, Read read, Checks const&... checks) const {
        auto const error_count{context_.errors.size()};
        read();
        if (context_.errors.size() == error_count) {
            (checks.validate(destination, context_.path, context_.errors), ...);
        }
    }

    DecodeContext context_;
};
}
