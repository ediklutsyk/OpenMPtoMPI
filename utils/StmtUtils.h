#ifndef STMT_UTILS_H
#define STMT_UTILS_H

#include <regex>
#include "llvm/ADT/StringRef.h"
#include "clang/AST/StmtOpenMP.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"


using namespace clang;

const ForStmt *getUnderlyingForStmt(const Stmt *statement);

const FunctionDecl *getEnclosingFunctionDecl(const Stmt *statement, ASTContext &context);

std::string getCodeBlockWithoutBraces(const std::string &InnerCode);

#endif //STMT_UTILS_H
