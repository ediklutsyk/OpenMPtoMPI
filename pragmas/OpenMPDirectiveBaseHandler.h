#ifndef OPENMP_DIRECTIVE_BASE_HANDLER_H
#define OPENMP_DIRECTIVE_BASE_HANDLER_H

#include "clang/Lex/Lexer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Tooling/Core/Replacement.h"
#include "../utils/VariablesHandler.h"
#include "../utils/VariableAnalyzer.h"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

class OpenMPDirectiveBaseHandler {
public:
    OpenMPDirectiveBaseHandler(const MatchFinder::MatchResult &Result,
                               VariablesHandler &VariablesHandler)
            : SM(*Result.SourceManager), LangOpts(Result.Context->getLangOpts()), Context(*Result.Context),
              Variables(VariablesHandler) {}

    virtual ~OpenMPDirectiveBaseHandler() = default;

    virtual std::string handle(const OMPExecutableDirective *OMPDir) = 0;

protected:
    SourceManager &SM;
    const LangOptions &LangOpts;
    ASTContext &Context;
    VariablesHandler &Variables;

    [[nodiscard]] std::string getSourceTextFromRange(const clang::SourceRange &range) const {
        return Lexer::getSourceText(CharSourceRange::getTokenRange(range), SM, LangOpts).str();
    }

    auto analyzeVariables(Stmt *statement) const {
        VariableAnalyzer Analyzer(Context, statement);
        Analyzer.TraverseStmt(statement);
        return Analyzer.getVariables();
    }

};

#endif // OPENMP_DIRECTIVE_BASE_HANDLER_H
