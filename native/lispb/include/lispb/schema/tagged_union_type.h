#pragma once

#include <lispb/schema/resolved_type_ref.h>
#include <lispb/schema/tagged_union_alternative.h>

#include <vector>

namespace lispb::schema {
struct TaggedUnionType {
    ResolvedTypeRef discriminant;
    std::vector<TaggedUnionAlternative> alternatives;
};
}
