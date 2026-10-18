#pragma once

#include <lispb/strings.h>

namespace lispb {

class TemporaryStringScope {
  public:
    explicit TemporaryStringScope(Strings& strings)
        : strings_{strings}
        , previous_{strings.temporary_} {
        strings_.temporary_ = true;
    }
    ~TemporaryStringScope() { strings_.temporary_ = previous_; }
    TemporaryStringScope(TemporaryStringScope const&) = delete;
    auto operator=(TemporaryStringScope const&) -> TemporaryStringScope& = delete;
  private:
    Strings& strings_;
    bool previous_;
};

}
