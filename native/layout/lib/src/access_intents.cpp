#include "access_intents.hpp"
namespace ioj::layout::analysis_detail {
auto access_includes_read(AccessOperation operation) -> bool {
    return operation != AccessOperation::write;
}
auto access_includes_write(AccessOperation operation) -> bool {
    return operation != AccessOperation::read;
}
}
