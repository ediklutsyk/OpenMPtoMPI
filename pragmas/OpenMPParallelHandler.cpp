#include "clang/Lex/Lexer.h"
#include "OpenMPDirectiveBaseHandler.h"

class OpenMPParallelHandler : public OpenMPDirectiveBaseHandler {
public:
    OpenMPParallelHandler(const MatchFinder::MatchResult &Result, VariablesHandler &VariablesHandler)
            : OpenMPDirectiveBaseHandler(Result, VariablesHandler) {
    }

    std::string handle(const OMPExecutableDirective *OMPDir) override {
        // Для omp parallel просто видаляємо директиву, залишаючи код всередині
        const Stmt *AssociatedStmt = OMPDir->getAssociatedStmt();
        if (!AssociatedStmt) {
            llvm::errs() << "Відсутній асоційований блок коду для omp parallel!\n";
            return "";
        }
        llvm::StringRef InnerCode = Lexer::getSourceText(
                CharSourceRange::getTokenRange(AssociatedStmt->getSourceRange()),
                SM, LangOpts);
        return getCodeBlockWithoutBraces(InnerCode.str());
    }

    std::string directiveName() override {
        return "#pragma omp parallel";
    }

};