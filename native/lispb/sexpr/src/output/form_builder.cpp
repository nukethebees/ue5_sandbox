#include <codegen/sexpr/form_builder.h>

namespace lispb::codegen::sexpr {
FormBuilder::FormBuilder(SExprIR& ir, Expr head, std::initializer_list<Expr> positional)
    : ir_{ir}
    , header_{head} {
    header_.insert(header_.end(), positional.begin(), positional.end());
}
auto FormBuilder::field(std::string_view key, Expr value) -> FormBuilder& {
    rows_.push_back({static_cast<std::uint32_t>(children_.size()), 2});
    children_.push_back(ir_.atom(":" + std::string{key}));
    children_.push_back(value);
    return *this;
}
auto FormBuilder::add(Expr child) -> FormBuilder& {
    rows_.push_back({static_cast<std::uint32_t>(children_.size()), 1});
    children_.push_back(child);
    return *this;
}
auto FormBuilder::build() -> Expr {
    return ir_.save_list(header_, children_, rows_);
}

}
