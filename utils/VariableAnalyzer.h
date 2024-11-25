#ifndef VARIABLE_ANALYZER_H
#define VARIABLE_ANALYZER_H

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/AST/ASTContext.h"

using namespace clang;
using namespace clang::tooling;

class VariableAnalyzer : public clang::RecursiveASTVisitor<VariableAnalyzer> {
public:
    struct VariableInfo {
        std::string name;
        clang::QualType type;
        bool isRead = false;
        bool isWritten = false;
        bool isLocal = false;
    };

    explicit VariableAnalyzer(clang::ASTContext &context, const clang::Stmt *statement);

    bool VisitDeclRefExpr(clang::DeclRefExpr *expr);

    bool VisitArraySubscriptExpr(clang::ArraySubscriptExpr *expr);

    [[nodiscard]] const std::map<std::string, VariableInfo> &getVariables() const;

private:
    clang::ASTContext &context;
    const clang::Stmt *statement;
    std::map<std::string, VariableInfo> variables;

    bool isDeclaredInStmt(const clang::VarDecl *var);

    bool isLeftHandSideOfAssignment(clang::Expr *expr);

    void checkReadOrWrite(clang::Expr *expr, VariableInfo &varInfo);
};

#endif // VARIABLE_ANALYZER_H
