#include "StmtUtils.h"

using namespace clang;

const ForStmt *getUnderlyingForStmt(const Stmt *Statement) {
    // Розмотуємо всі обгортки, поки не знайдемо ForStmt
    while (Statement) {
        if (const auto *ForLoop = dyn_cast<ForStmt>(Statement)) {
            return ForLoop;
        } else if (const auto *CompoundStatement = dyn_cast<CompoundStmt>(Statement)) {
            // Якщо CompoundStmt містить один оператор, продовжуємо розмотувати
            if (CompoundStatement->size() == 1) {
                Statement = *(CompoundStatement->body_begin());
                continue;
            } else {
                // Якщо більше одного оператора, це не те, що ми шукаємо
                return nullptr;
            }
        } else if (const auto *AttributedStatement = dyn_cast<AttributedStmt>(Statement)) {
            Statement = AttributedStatement->getSubStmt();
            continue;
        } else if (const auto *CapturedStatement = dyn_cast<CapturedStmt>(Statement)) {
            Statement = CapturedStatement->getCapturedStmt();
            continue;
        } else if (const auto *OMPDirective = dyn_cast<OMPExecutableDirective>(Statement)) {
            Statement = OMPDirective->getAssociatedStmt();
            continue;
        } else {
            // Якщо не можемо розпізнати оператор, повертаємо nullptr
            return nullptr;
        }
    }
    return nullptr;
}


const FunctionDecl *getEnclosingFunctionDecl(const Stmt *statement, ASTContext &context) {
    const Stmt *currentStmt = statement;
    while (currentStmt) {
        const auto &parents = context.getParents(*currentStmt);
        if (parents.empty()) {
            break;
        }
        if (const auto *functionDecl = parents[0].get<FunctionDecl>()) {
            return functionDecl;
        }
        currentStmt = parents[0].get<Stmt>();
    }
    return nullptr;
}


std::string trim(const std::string &str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }
    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    return {start, end + 1};
}

std::string getCodeBlockWithoutBraces(const std::string& InnerCode) {
    std::regex braceRegex(R"(^\s*\{([\s\S]*)\}\s*$)");
    std::smatch match;
    if (std::regex_match(InnerCode, match, braceRegex)) {
        return trim(match[1].str());
    }
    return trim(InnerCode);
}