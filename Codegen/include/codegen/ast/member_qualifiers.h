#pragma once

namespace codegen {

struct MemberQualifiers {
    bool is_inline{false};
    bool is_static{false};
    bool is_constexpr{false};
};

} // namespace codegen
