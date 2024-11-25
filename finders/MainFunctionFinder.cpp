#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/Core/Replacement.h"
#include "clang/Lex/Lexer.h"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

class MainFunctionFinder : public MatchFinder::MatchCallback {
public:
    explicit MainFunctionFinder(Replacements &R) : Replaces(R) {}

    void run(const MatchFinder::MatchResult &Result) override {
        const auto *mainFunc = Result.Nodes.getNodeAs<FunctionDecl>("mainFunction");
        if (!mainFunc || !mainFunc->hasBody()) {
            return;
        }
        const Stmt *mainBody = mainFunc->getBody();
        std::string initCode;
        if (mainFunc->getNumParams() == 2) {
            const ParmVarDecl *firstArg = mainFunc->getParamDecl(0);
            const ParmVarDecl *secondArg = mainFunc->getParamDecl(1);
            if (firstArg != nullptr && secondArg != nullptr) {
                initCode += "\nMPI_Init(&" + firstArg->getNameAsString() + ", &" + secondArg->getNameAsString() + ");";
            }
        } else {
            initCode += "\nMPI_Init(NULL, NULL);";
        }
        initCode += "\nMPI_Comm_rank(MPI_COMM_WORLD, &world_rank);";
        initCode += "\nMPI_Comm_size(MPI_COMM_WORLD, &world_size);";
        SourceLocation FuncStart = Lexer::getLocForEndOfToken(mainFunc->getBody()->getBeginLoc(), 0,
                                                              *Result.SourceManager, Result.Context->getLangOpts());
        auto error = Replaces.add(Replacement(*Result.SourceManager, FuncStart, 0, initCode));
        if (error) {
            llvm::errs() << "Помилка додавання MPI_Init: " << llvm::toString(std::move(error)) << "\n";
        }
        // Знаходимо останній return 0;
        const ReturnStmt *lastReturnStmt = nullptr;
        for (const Stmt *child: mainBody->children()) {
            if (const auto *currentReturn = dyn_cast<ReturnStmt>(child)) {
                const Expr *value = currentReturn->getRetValue();
                if (value && isa<IntegerLiteral>(value) && cast<IntegerLiteral>(value)->getValue() == 0) {
                    lastReturnStmt = currentReturn;
                }
            }
        }
        if (lastReturnStmt) {
            SourceLocation RetLoc = lastReturnStmt->getBeginLoc();
            std::string mpiFinalizeCode = "MPI_Finalize();\n";
            error = Replaces.add(Replacement(*Result.SourceManager, RetLoc, 0, mpiFinalizeCode));
            if (error) {
                llvm::errs() << "Помилка додавання MPI_Finalize: " << llvm::toString(std::move(error)) << "\n";
            }
        }
    }

private:
    Replacements &Replaces;
};
