#include "LoopConditionCallCheck.hpp"
#include "LoopViewAccessorCallCheck.hpp"
#include "LoopViewConstructionCheck.hpp"
#include "NoPairCheck.hpp"

#include <clang-tidy/ClangTidyModule.h>

namespace clang::tidy::ioj {

class IojTidyModule : public ClangTidyModule {
  public:
    void addCheckFactories(ClangTidyCheckFactories& factories) override {
        factories.registerCheck<LoopConditionCallCheck>("ioj-loop-condition-call");
        factories.registerCheck<LoopViewConstructionCheck>("ioj-loop-view-construction");
        factories.registerCheck<LoopViewAccessorCallCheck>("ioj-loop-view-accessor-call");
        factories.registerCheck<NoPairCheck>("ioj-no-pair");
    }
};

static ClangTidyModuleRegistry::Add<IojTidyModule> registration{
    "ioj-module", "Project-specific simulation checks."};

} // namespace clang::tidy::ioj
