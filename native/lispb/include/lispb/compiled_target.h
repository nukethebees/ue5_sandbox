#pragma once
#include <lispb/compilation.h>
#include <lispb/publication_options.h>
namespace lispb {
struct CompiledTarget {
    Compilation compilation;
    PublicationOptions publication;
};
}
