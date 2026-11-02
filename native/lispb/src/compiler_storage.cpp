#include <lispb/compiler_storage.h>

namespace lispb {

void CompilerStorage::reset() {
    diagnostics.clear();
    registered_types.clear();
    declarations.clear();
    syntax.clear();
    sources.clear();
    strings.clear();
    cpp_types.clear();
}

} // namespace lispb
