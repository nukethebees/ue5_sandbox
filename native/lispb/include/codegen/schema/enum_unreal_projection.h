#pragma once

#include <codegen/schema/enum_reflection.h>

#include <filesystem>
#include <string>

namespace codegen {

struct EnumUnrealProjection {
    std::string name;
    std::filesystem::path header;
    std::string header_include;
    std::filesystem::path conversion_header;
    std::string native_header_include;
    EnumReflection reflection{EnumReflection::uenum};
};

} // namespace codegen
