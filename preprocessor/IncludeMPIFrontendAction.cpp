#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Core/Replacement.h"
#include "clang/Frontend/FrontendActions.h"

#include "IncludeMPIHeaderCallback.cpp"

using namespace clang;
using namespace clang::tooling;

class IncludeMPIFrontendAction : public PreprocessOnlyAction {
public:
    IncludeMPIFrontendAction(Replacements &R) : Replaces(R) {}

    void ExecuteAction() override {
        clang::Preprocessor &PP = getCompilerInstance().getPreprocessor();
        std::unique_ptr<IncludeMPIHeaderCallback> Callback =
                std::make_unique<IncludeMPIHeaderCallback>(PP, Replaces);
        PP.addPPCallbacks(std::move(Callback));
    }

private:
    Replacements &Replaces;
};