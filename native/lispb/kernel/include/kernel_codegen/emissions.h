#pragma once

#include <codegen/sexpr/source_span.h>
#include <kernel_codegen/indices.h>
#include <lispb/indices.h>
#include <lispb/kernel_profile.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace kernel_codegen::detail {
class Emissions {
  public:
    auto add(lispb::KernelProfile profile, lispb::codegen::sexpr::SourceSpan span) -> EmissionId;
    auto profile(EmissionId const id) const -> lispb::KernelProfile { return profile_[id.value]; }
    auto span(EmissionId const id) const -> lispb::codegen::sexpr::SourceSpan {
        return span_[id.value];
    }
    auto header(EmissionId const id) const -> lispb::StringId { return header_[id.value]; }
    auto header(EmissionId const id) -> lispb::StringId& { return header_[id.value]; }
    auto source(EmissionId const id) const -> lispb::StringId { return source_[id.value]; }
    auto source(EmissionId const id) -> lispb::StringId& { return source_[id.value]; }
    auto tests(EmissionId const id) const -> std::optional<lispb::StringId> {
        return tests_[id.value];
    }
    auto tests(EmissionId const id) -> std::optional<lispb::StringId>& { return tests_[id.value]; }
    auto header_include(EmissionId const id) const -> lispb::StringId {
        return header_include_[id.value];
    }
    auto header_include(EmissionId const id) -> lispb::StringId& {
        return header_include_[id.value];
    }
    auto cpp_namespace(EmissionId const id) const -> lispb::StringId {
        return cpp_namespace_[id.value];
    }
    auto cpp_namespace(EmissionId const id) -> lispb::StringId& { return cpp_namespace_[id.value]; }
    auto export_specifier(EmissionId const id) const -> lispb::StringId {
        return export_specifier_[id.value];
    }
    auto export_specifier(EmissionId const id) -> lispb::StringId& {
        return export_specifier_[id.value];
    }
    auto size() const -> std::size_t { return profile_.size(); }
    void clear();
  private:
    std::vector<lispb::KernelProfile> profile_;
    std::vector<lispb::codegen::sexpr::SourceSpan> span_;
    std::vector<lispb::StringId> header_;
    std::vector<lispb::StringId> source_;
    std::vector<std::optional<lispb::StringId>> tests_;
    std::vector<lispb::StringId> header_include_;
    std::vector<lispb::StringId> cpp_namespace_;
    std::vector<lispb::StringId> export_specifier_;
};
}
