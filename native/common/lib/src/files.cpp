#include <ioj/files.h>

#include <cerrno>
#include <fstream>
#include <iterator>

namespace ioj {
auto read_file(std::filesystem::path const& path) -> std::expected<std::string, FileReadError> {
    errno = 0;
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::unexpected{
            FileReadError{FileReadErrorCode::OpenFailed, path, {errno, std::generic_category()}}};
    }

    std::string result{std::istreambuf_iterator<char>{input}, {}};
    if (input.bad()) {
        return std::unexpected{
            FileReadError{FileReadErrorCode::ReadFailed, path, {errno, std::generic_category()}}};
    }

    return result;
}
auto path_to_utf8(std::filesystem::path const& path) -> std::string {
    auto const bytes{path.u8string()};
    return {reinterpret_cast<char const*>(bytes.data()), bytes.size()};
}
}
