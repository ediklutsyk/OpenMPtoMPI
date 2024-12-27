#include "clang/Tooling/Core/Replacement.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/StmtOpenMP.h"
#include "clang/Lex/Lexer.h"

#include "../pragmas/OpenMPParallelForHandler.cpp"
#include "../pragmas/OpenMPParallelHandler.cpp"
#include "../pragmas/OpenMPSingleHandler.cpp"
#include "../pragmas/OpenMPMasterHandler.cpp"
#include "../pragmas/OpenMPBarrierHandler.cpp"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;


class OpenMPPragmaFinder : public MatchFinder::MatchCallback {
public:
    explicit OpenMPPragmaFinder(Replacements &R) : Replace(R) {}

    void run(const MatchFinder::MatchResult &Result) override {
        const auto *OMPDir = Result.Nodes.getNodeAs<OMPExecutableDirective>("ompDirective");
        if (!OMPDir) {
            llvm::errs() << "Директива OpenMP не знайдена!\n";
            return;
        }
        const auto &SM = *Result.SourceManager;
        const LangOptions &LangOpts = Result.Context->getLangOpts();

        // Отримуємо ім'я поточної функції
        Variables.setCurrentFunction(OMPDir, *Result.Context);

        // Отримуємо діапазон директиви та асоційованого коду
        SourceRange ReplaceRange = getDirectiveRange(OMPDir, Result);
        // Перевіряємо валідність діапазону
        if (!ReplaceRange.isValid()) {
            llvm::errs() << "Невалідний діапазон заміни!\n";
            return;
        }

        // Генеруємо код MPI
        std::string MPICode;
        std::unique_ptr<OpenMPDirectiveBaseHandler> Handler;
        if (isa<OMPParallelDirective>(OMPDir)) {
            Handler = std::make_unique<OpenMPParallelHandler>(Result, Variables);
        } else if (isa<OMPParallelForDirective>(OMPDir)) {
            Handler = std::make_unique<OpenMPParallelForHandler>(Result, Variables);
        } else if (isa<OMPSingleDirective>(OMPDir)) {
            Handler = std::make_unique<OpenMPSingleHandler>(Result, Variables);
        } else if (isa<OMPMasterDirective>(OMPDir)) {
            Handler = std::make_unique<OpenMPMasterHandler>(Result, Variables);
        } else if (isa<OMPBarrierDirective>(OMPDir)) {
            Handler = std::make_unique<OpenMPBarrierHandler>(Result, Variables);
        } else {
            llvm::errs() << "Непідтримувана директива OpenMP для перетворення!\n";
            return;
        }
        if (Handler) {
            std::string Code = Handler->handle(OMPDir);
            if (Code.empty()) {
                llvm::errs() << "Помилка обробки директиви OpenMP!\n";
                return;
            }
            MPICode += Code;
        }

        // Додаємо заміну через Replacements
        auto Err = Replace.add(Replacement(SM, CharSourceRange::getCharRange(ReplaceRange), MPICode));
        if (Err) {
            llvm::errs() << "Помилка додавання заміни: " << llvm::toString(std::move(Err)) << "\n";
        } else {
            llvm::errs() << "Заміна директиви: " << Handler->directiveName() << " додана успішно\n";
        }
    }

    static SourceRange getDirectiveRange(const OMPExecutableDirective *OMPDir, const MatchFinder::MatchResult &Result) {
        SourceLocation StartLoc = OMPDir->getBeginLoc();
        SourceLocation EndLoc;
        if (OMPDir->hasAssociatedStmt()) {
            const Stmt *AssociatedStmt = OMPDir->getAssociatedStmt();
            EndLoc = AssociatedStmt->getEndLoc();
        } else {
            EndLoc = OMPDir->getEndLoc();
        }
        // Збільшуємо EndLoc на 1 символ для захоплення закриваючої дужки
        EndLoc = Lexer::getLocForEndOfToken(EndLoc, 0, *Result.SourceManager, Result.Context->getLangOpts());
        return {StartLoc, EndLoc};
    }

private:
    Replacements &Replace;
    VariablesHandler Variables;
};
