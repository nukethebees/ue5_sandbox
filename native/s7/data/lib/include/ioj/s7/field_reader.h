#pragma once

#include <ioj/s7/reader.h>

namespace ioj::s7 {
class FieldReader {
  public:
    FieldReader(Reader const& reader,
                NodeIndex node,
                std::string_view path,
                AstDiagnostics& errors,
                ReadMode mode = ReadMode::Create)
        : reader_{reader}
        , node_{node}
        , path_{path}
        , errors_{errors}
        , mode_{mode} {}

    template <typename T, typename... Checks>
    void operator()(T& destination, Checks const&... checks) const {
        read_checked(
            destination,
            [&] { decode_field(reader_, node_, path_, destination, mode_, errors_); },
            checks...);
    }

    template <typename T, typename... Checks>
    void construct(std::optional<T>& destination, Checks const&... checks) const {
        read_checked(
            destination,
            [&] { read_field(reader_, node_, path_, destination, errors_); },
            checks...);
    }

    template <typename T, typename... Checks>
    void patch(std::optional<T>& destination, Checks const&... checks) const {
        read_checked(
            destination,
            [&] { decode_patch_field(reader_, node_, path_, destination, errors_); },
            checks...);
    }
  private:
    template <typename T, typename Read, typename... Checks>
    void read_checked(T& destination, Read read, Checks const&... checks) const {
        auto const error_count{errors_.size()};
        read();
        if (errors_.size() == error_count) {
            (checks.validate(destination, path_, errors_), ...);
        }
    }

    Reader const& reader_;
    NodeIndex node_;
    std::string_view path_;
    AstDiagnostics& errors_;
    ReadMode mode_;
};
}
