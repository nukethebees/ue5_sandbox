#include "LoopConditionCallCheck.hpp"
#include "LoopViewConstructionCheck.hpp"

#include <clang-tidy/ClangTidyModule.h>

namespace clang::tidy::ioj {

class IojTidyModule : public ClangTidyModule {
  public:
    void addCheckFactories(ClangTidyCheckFactories& factories) override {
        factories.registerCheck<LoopConditionCallCheck>("ioj-loop-condition-call");
        factories.registerCheck<LoopViewConstructionCheck>("ioj-loop-view-construction");
    }
};

static ClangTidyModuleRegistry::Add<IojTidyModule> registration{
    "ioj-module", "Project-specific simulation checks."};

} // namespace clang::tidy::ioj
