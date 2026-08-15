#pragma once

#include <Containers/Array.h>
#include <HAL/Platform.h>

class UPrimitiveComponent;

namespace ml {
struct FSpatialQueryHit {
    UPrimitiveComponent const* component{nullptr};
    int32 item{INDEX_NONE};
};
}
