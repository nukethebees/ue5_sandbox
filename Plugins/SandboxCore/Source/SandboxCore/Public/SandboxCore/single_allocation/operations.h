#pragma once

#include <sandbox/core/single_allocation/operations.h>
#include <SandboxCore/single_allocation/runtime.h>

namespace ml::soa_storage {

using StorageOperations =
    soa_storage_detail::StorageOperations<int32, require, rounded_capacity, growth_capacity>;

}
