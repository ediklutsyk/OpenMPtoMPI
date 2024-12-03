#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Preprocessor.h"

#include "PreprocessorAndASTAction.cpp"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

// Фабрика для створення дії, яка поєднує обробку директив #include і MatchFinder
class PreprocessorAndASTActionFactory : public FrontendActionFactory {
public:
    PreprocessorAndASTActionFactory(MatchFinder &Finder, Replacements &Replaces) : Replaces(Replaces), Finder(Finder) {}

    std::unique_ptr<clang::FrontendAction> create() override {
        return std::make_unique<PreprocessorAndASTAction>(Finder, Replaces);
    }

private:
    MatchFinder &Finder;
    Replacements &Replaces;
};