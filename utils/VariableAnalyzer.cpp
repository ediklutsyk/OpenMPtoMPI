#include "VariableAnalyzer.h"

VariableAnalyzer::VariableAnalyzer(ASTContext &context, const Stmt *statement)
        : context(context), statement(statement) {}

bool VariableAnalyzer::VisitDeclRefExpr(DeclRefExpr *expr) {
    if (auto var = dyn_cast<VarDecl>(expr->getDecl())) {
        std::string varName = var->getNameAsString();
        auto &varInfo = variables[varName];
        varInfo.name = varName;
        varInfo.type = var->getType();
        // Перевірка локальності змінної: чи оголошена змінна в межах аналізованого блоку
        varInfo.isLocal = isDeclaredInStmt(var);
        // Ігноруємо масиви на цьому рівні, оскільки вони будуть оброблені в іншому методі
        if (var->getType()->isArrayType()) {
            return true;
        }
        // Перевіряємо, чи змінна використовується для читання або запису
        checkReadOrWrite(expr, varInfo);
    }
    return true;
}

bool VariableAnalyzer::VisitArraySubscriptExpr(ArraySubscriptExpr *expr) {
    // Отримуємо базовий вираз масиву, ігноруючи касти або дужки
    auto *baseExpr = expr->getBase()->IgnoreParenImpCasts();
    if (auto arrayRef = dyn_cast<DeclRefExpr>(baseExpr)) {
        std::string varName = arrayRef->getDecl()->getNameAsString();
        auto &varInfo = variables[varName];
        varInfo.name = varName;
        varInfo.type = arrayRef->getType();

        // Перевіряємо, чи цей вираз є лівою частиною присвоєння
        if (isLeftHandSideOfAssignment(expr)) {
            varInfo.isWritten = true;
        } else {
            varInfo.isRead = true;
        }
    }
    return true;
}

bool VariableAnalyzer::isDeclaredInStmt(const VarDecl *var) {
    SourceLocation varLoc = var->getBeginLoc();
    SourceLocation stmtStartLoc = statement->getBeginLoc();
    SourceLocation stmtEndLoc = statement->getEndLoc();

    return varLoc >= stmtStartLoc && varLoc <= stmtEndLoc;
}

bool VariableAnalyzer::isLeftHandSideOfAssignment(Expr *expr) {
    auto parents = context.getParents(*expr);
    if (!parents.empty()) {
        if (const auto *binOp = parents[0].get<BinaryOperator>()) {
            // Перевіряємо, чи це оператор присвоєння, і чи вираз ліворуч
            return binOp->isAssignmentOp() && binOp->getLHS() == expr;
        }
    }
    return false;
}

void VariableAnalyzer::checkReadOrWrite(Expr *expr, VariableInfo &varInfo) {
    if (isLeftHandSideOfAssignment(expr)) {
        varInfo.isWritten = true;
    } else {
        varInfo.isRead = true;
    }
}

const std::map<std::string, VariableAnalyzer::VariableInfo> &VariableAnalyzer::getVariables() const {
    return variables;
}
