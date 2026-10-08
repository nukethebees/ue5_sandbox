#include <ioj/files.h>

#include <array>
#include <cerrno>
#include <fstream>

namespace ioj {
auto read_file(std::filesystem::path const& path) -> std::expected<std::string, FileReadError> {
    errno = 0;
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::unexpected{
            FileReadError{FileReadErrorCode::OpenFailed, path, {errno, std::generic_category()}}};
    }

    std::string result;
    std::array<char, 8192> buffer;
    while (input.read(buffer.data(), buffer.size())) {
        result.append(buffer.data(), buffer.size());
    }
    result.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
    if (!input.eof() || input.bad()) {
        return std::unexpected{FileReadError{FileReadErrorCode::ReadFailed,
                                             path,
                                             errno ? std::error_code{errno, std::generic_category()}
                                                   : std::make_error_code(std::errc::io_error)}};
    }

    return result;
}
auto path_to_utf8(std::filesystem::path const& path) -> std::string {
    auto const bytes{path.u8string()};
    return {reinterpret_cast<char const*>(bytes.data()), bytes.size()};
}
}
