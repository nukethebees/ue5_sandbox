#pragma once

#include <codegen/ast/node_id.h>

#include <cstdint>

namespace lispb::codegen {

class CppIR;

class RecordId {
  public:
    operator NodeId() const { return NodeId{value_}; }
    auto index() const -> std::uint32_t { return value_ & NodeId::index_mask; }
    auto operator==(RecordId const&) const -> bool = default;
  private:
    friend class CppIR;
    explicit RecordId(std::uint32_t value)
        : value_{value} {}
    std::uint32_t value_;
};

static_assert(sizeof(RecordId) == 4);

}
