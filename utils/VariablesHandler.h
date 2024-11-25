#ifndef VARIABLES_HANDLER_H
#define VARIABLES_HANDLER_H

#include "clang/AST/ASTContext.h"
#include "clang/AST/Stmt.h"
#include "StmtUtils.h"

class VariablesHandler {
public:
    explicit VariablesHandler() = default;

    void setCurrentFunction(const clang::Stmt *statement, clang::ASTContext &context);

    std::string useOrInitVariable(const std::string &varName, const std::string &typeName);

protected:
    std::map<std::string, std::set<std::string>> declaredVariables;
    std::string functionName;
};

#endif // VARIABLES_HANDLER_H
