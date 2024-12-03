#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Preprocessor.h"

#include "../preprocessor/PreprocessorHeaderCallback.cpp"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

// Клас для обробки включень і поєднання з ASTMatcher
class PreprocessorAndASTAction : public ASTFrontendAction {
public:
    PreprocessorAndASTAction(MatchFinder &Finder, Replacements &Replaces) : Replaces(Replaces), Finder(Finder) {}

    // Додаємо PPCallbacks для аналізу директив #include
    bool BeginSourceFileAction(clang::CompilerInstance &CI) override {
        CI.getPreprocessor().addPPCallbacks(
                std::make_unique<PreprocessorHeaderCallback>(CI.getPreprocessor(), Replaces));
        return true;
    }

    // Додаємо ASTMatcher для аналізу директив OpenMP
    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(CompilerInstance &CI, llvm::StringRef InFile) override {
        return Finder.newASTConsumer();
    }

private:
    MatchFinder &Finder;
    Replacements &Replaces;
};