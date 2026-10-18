#pragma once

#include <lispb/indices.h>

#include <stdexcept>

namespace lispb::codegen::sexpr {
class Sources;

class SourceError final : public std::runtime_error {
  public:
    SourceError(Diagnostics const& diagnostics, Sources const& sources, DiagnosticId diagnostic);
    auto diagnostic() const -> DiagnosticId { return diagnostic_; }
  private:
    DiagnosticId diagnostic_;
};

}
