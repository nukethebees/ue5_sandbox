#pragma once

namespace lispb::codegen {

struct CppFileDefinitionFlags {
    bool pragma_once{true};
    bool clang_format_off{};
    bool format_generated{};
};

}
