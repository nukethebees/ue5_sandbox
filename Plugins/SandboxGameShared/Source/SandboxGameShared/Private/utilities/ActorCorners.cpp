#include "SandboxGameShared/utilities/ActorCorners.h"

namespace ioj {
auto FActorCorners::to_string() const -> FString {
    FString out{};

    for (auto const& corner : data) {
        out += FString::Printf(TEXT("%s\n"), *corner.ToString());
    }

    return out;
}

} // namespace ioj
