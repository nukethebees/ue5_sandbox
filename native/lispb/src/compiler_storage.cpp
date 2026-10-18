#include <lispb/compiler_storage.h>

namespace lispb {

void CompilerStorage::reset() {
    diagnostics.clear();
    registered_types.clear();
    declarations.clear();
    cpp_types.clear();
    syntax.clear();
    sources.clear();
    strings.clear();
}

} // namespace lispb
