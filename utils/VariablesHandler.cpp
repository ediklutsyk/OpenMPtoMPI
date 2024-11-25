#include "VariablesHandler.h"

void VariablesHandler::setCurrentFunction(const clang::Stmt *statement, clang::ASTContext &context) {
    auto FuncDecl = getEnclosingFunctionDecl(statement, context);
    functionName = FuncDecl ? FuncDecl->getNameAsString() : "global";
    if (declaredVariables.find(functionName) == declaredVariables.end()) {
        declaredVariables[functionName] = std::set<std::string>();
    }
}

std::string VariablesHandler::useOrInitVariable(const std::string &varName, const std::string &typeName) {
    auto &varsInFunction = declaredVariables[functionName];
    if (varsInFunction.find(varName) == varsInFunction.end()) {
        varsInFunction.insert(varName);
        return typeName + " " + varName;
    } else {
        return varName;
    }
}
