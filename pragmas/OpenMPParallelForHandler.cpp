#include "OpenMPDirectiveBaseHandler.h"

class OpenMPParallelForHandler : public OpenMPDirectiveBaseHandler {
public:
    OpenMPParallelForHandler(const MatchFinder::MatchResult &Result, VariablesHandler &VariablesHandler)
            : OpenMPDirectiveBaseHandler(Result, VariablesHandler) {
    }

    std::string handle(const OMPExecutableDirective *OMPDir) override {
        std::string MPICode;

        const Stmt *associatedStmt = OMPDir->getAssociatedStmt();
        const ForStmt *For = getUnderlyingForStmt(associatedStmt);
        if (!For) {
            llvm::errs() << "Директива omp parallel for не асоційована з циклом for!\n";
            return "";
        }
        const auto &variablesInfo = analyzeVariables((Stmt *) For);

        // Ініціалізуємо код для передачі даних
        std::string dataTransferCode;
        std::string beforeLoopCode;
        std::string afterLoopCode;
        std::map<std::string, std::string> ReplaceMap;
        // Обробка змінних
        for (const auto &varEntry: variablesInfo) {
            const std::string &varName = varEntry.first;
            const auto &varInfo = varEntry.second;
            std::string varTypeStr = varInfo.type.getAsString();
            if (varInfo.isLocal) {
                // Ігноруємо локальні змінні, оголошені всередині циклу
                continue;
            }
            if (varInfo.type->isArrayType() || varInfo.type->isPointerType()) {
                std::string arrayBaseType = varInfo.type->getAsArrayTypeUnsafe()->getElementType().getAsString();
                if (varInfo.isRead && !varInfo.isWritten) {
                    // Випадок 1: Масив використовується для читання (arr[i])
                    dataTransferCode += "// Розподіл масиву " + varName + " між процесами\n";

                    // Оголошуємо локальний буфер для отримання даних
                    std::string LocalVarDecl = Variables.useOrInitVariable("local_" + varName, arrayBaseType);
                    dataTransferCode += LocalVarDecl + " [chunk];\n";

                    // Генеруємо код для MPI_Scatter
                    dataTransferCode += "MPI_Scatter(" + varName + ", chunk, MPI_INT, local_" + varName +
                                        ", chunk, MPI_INT, 0, MPI_COMM_WORLD);\n";

                    // Додаємо до ReplaceMap
                    ReplaceMap[varName] = "local_" + varName;
                } else if (!varInfo.isRead && varInfo.isWritten) {
                    // Випадок 2: Масив використовується для запису (res[i] = ...)
                    dataTransferCode +=
                            "// Підготовка локального буфера для запису результатів у масив " + varName + "\n";

                    std::string LocalVarDecl = Variables.useOrInitVariable("local_" + varName, arrayBaseType);
                    dataTransferCode += LocalVarDecl + " [chunk];\n";

                    // Додаємо до ReplaceMap
                    ReplaceMap[varName] = "local_" + varName;

                    // Після циклу додаємо код для MPI_Gather
                    afterLoopCode += "// Збір результатів з процесів у масив " + varName + "\n";
                    afterLoopCode += "MPI_Gather(" + ReplaceMap[varName] + ", chunk, MPI_INT, " + varName +
                                     ", chunk, MPI_INT, 0, MPI_COMM_WORLD);\n";
                } else if (varInfo.isRead && varInfo.isWritten) {
                    // Масив читається і записується всередині циклу
                    dataTransferCode += "/* Змінна " + varName + " читається і записується в циклі. \n";
                    dataTransferCode += "   Потребує складної обробки для синхронізації між процесами. */\n";
                }
            } else if (varInfo.type->isBuiltinType()) {
                if (varInfo.isRead && !varInfo.isWritten) {
                    // Випадок 3: Скалярна змінна використовується для читання і ініціалізована поза циклом
                    dataTransferCode += "// Скалярна змінна " + varName + " використовується для читання\n";
                    dataTransferCode += "MPI_Bcast(&" + varName + ", 1, MPI_INT, 0, MPI_COMM_WORLD);\n";
                } else if (varInfo.isWritten) {
                    // Змінна модифікується в циклі
                    dataTransferCode += "/* Скалярна змінна " + varName + " модифікується в циклі.\n";
                    dataTransferCode += "   Розгляньте можливість використання MPI_Reduce для збору результатів. */\n";
                }
            } else {
                // Інші типи змінних
                dataTransferCode += "/* Змінна " + varName + " має складний тип (" + varTypeStr +
                                    ") і потребує ручної обробки.\n";
                dataTransferCode += "   Рекомендується переглянути використання цієї змінної та адаптувати код відповідно. */\n";
            }
        }

        // Отримуємо початкове значення ітератора (global_start)
        MPICode += Variables.useOrInitVariable("global_start", "int") + " = ";
        if (const auto *Init = For->getInit()) {
            if (const auto *DS = dyn_cast<DeclStmt>(Init)) {
                if (const auto *VD = dyn_cast<VarDecl>(DS->getSingleDecl())) {
                    if (const auto *InitExpr = VD->getInit()) {
                        llvm::StringRef InitCode = Lexer::getSourceText(
                                CharSourceRange::getTokenRange(InitExpr->getSourceRange()), SM, LangOpts);
                        MPICode += InitCode.str();
                    } else {
                        MPICode += "/* невідомо */";
                    }
                }
            } else {
                llvm::StringRef InitCode = Lexer::getSourceText(
                        CharSourceRange::getTokenRange(Init->getSourceRange()), SM, LangOpts);
                MPICode += InitCode.str();
            }
        } else {
            MPICode += "0";
        }
        MPICode += ";\n";

        // Отримуємо кінцеве значення ітератора (global_end)
        MPICode += Variables.useOrInitVariable("global_end", "int") + " = ";
        if (const auto *Cond = For->getCond()) {
            if (const auto *BinOp = dyn_cast<BinaryOperator>(Cond)) {
                llvm::StringRef EndCode = Lexer::getSourceText(
                        CharSourceRange::getTokenRange(BinOp->getRHS()->getSourceRange()), SM, LangOpts);
                if (BinOp->getOpcode() == BO_LT ||
                    BinOp->getOpcode() == BO_GT) {
                    MPICode += EndCode.str();
                } else if (BinOp->getOpcode() == BO_LE) {
                    MPICode += "(" + EndCode.str() + " + 1)";
                } else if (BinOp->getOpcode() == BO_GE) {
                    MPICode += "(" + EndCode.str() + " - 1)";
                } else {
                    MPICode += "/* невідомо */";
                }
            } else {
                llvm::StringRef CondCode = Lexer::getSourceText(
                        CharSourceRange::getTokenRange(Cond->getSourceRange()), SM, LangOpts);
                MPICode += CondCode.str();
            }
        } else {
            MPICode += "/* невідомо */";
        }
        MPICode += ";\n";

        MPICode += Variables.useOrInitVariable("iterations", "int") + " = global_end - global_start;\n";
        MPICode += Variables.useOrInitVariable("chunk", "int") + " = iterations / world_size;\n";
        MPICode += Variables.useOrInitVariable("start", "int") + " = global_start + world_rank * chunk;\n";
        MPICode += Variables.useOrInitVariable("end", "int") +
                   " = (world_rank == world_size - 1) ? global_end : start + chunk;\n";

        // Перевіряємо наявність клаузули reduction
        const auto *ParallelForDir = dyn_cast<OMPParallelForDirective>(OMPDir);
        bool hasReduction = false;
        std::string ReductionOp;
        std::string ReductionVar;

        if (ParallelForDir) {
            ArrayRef<OMPClause *> Clauses = ParallelForDir->clauses();
            for (const auto *Clause: Clauses) {
                if (const auto *ReductionClause = dyn_cast<OMPReductionClause>(Clause)) {
                    hasReduction = true;

                    // Отримуємо змінні для редукції
                    for (auto VarIt = ReductionClause->varlist_begin();
                         VarIt != ReductionClause->varlist_end(); ++VarIt) {
                        const DeclRefExpr *RefExpr = dyn_cast<DeclRefExpr>(*VarIt);
                        if (RefExpr) {
                            ReductionVar = RefExpr->getDecl()->getNameAsString();
                        }
                    }

                    // Шукаємо оператор редукції в тексті
                    SourceRange ReductionRange(ReductionClause->getBeginLoc(), ReductionClause->getColonLoc());
                    llvm::StringRef ReductionCode = Lexer::getSourceText(
                            CharSourceRange::getTokenRange(ReductionRange), SM,
                            Context.getLangOpts());

                    if (ReductionCode.contains("+")) {
                        ReductionOp = "MPI_SUM";
                    } else if (ReductionCode.contains("*")) {
                        ReductionOp = "MPI_PROD";
                    } else if (ReductionCode.contains("min")) {
                        ReductionOp = "MPI_MIN";
                    } else if (ReductionCode.contains("max")) {
                        ReductionOp = "MPI_MAX";
                    } else {
                        llvm::errs() << "Невідомий оператор редукції в клаузулі: " << ReductionCode.str() << "\n";
                        return "";
                    }
                }
            }
        }

        MPICode += dataTransferCode + "\n";
        MPICode += beforeLoopCode + "\n";


        if (hasReduction) {
            std::string LocalReductionVarDecl = Variables.useOrInitVariable("local_" + ReductionVar, "int");
            MPICode += LocalReductionVarDecl + " = 0;\n";
            MPICode += "for (int i = start; i < end; ++i)";
            const Stmt *Body = For->getBody();
            llvm::StringRef BodyCode = Lexer::getSourceText(CharSourceRange::getTokenRange(Body->getSourceRange()),
                                                            SM, LangOpts);
            std::string ModifiedBodyCode = BodyCode.str();
            std::regex varRegex("\\b" + ReductionVar + "\\b");
            ModifiedBodyCode = std::regex_replace(ModifiedBodyCode, varRegex, "local_" + ReductionVar);
            MPICode += " " + ModifiedBodyCode + "\n";
            MPICode += "MPI_Reduce(&local_" + ReductionVar + ", &" + ReductionVar + ", 1, MPI_INT, " + ReductionOp +
                       ", 0, MPI_COMM_WORLD);\n";
        } else {
            std::string LoopBodyCode = generateLoopCodeWithReplacements(For->getBody(), ReplaceMap, SM, LangOpts, 0);
            MPICode += "for (int i = start; i < end; ++i)" + LoopBodyCode + "\n";
        }

        MPICode += afterLoopCode + "\n";
        return MPICode;
    }

private:
    std::string generateLoopCodeWithReplacements(const Stmt *Body, const std::map<std::string, std::string> &ReplaceMap,
                                                 const SourceManager &SM, const LangOptions &LangOpts, int StartIndex) {
        llvm::StringRef BodyCode = Lexer::getSourceText(CharSourceRange::getTokenRange(Body->getSourceRange()), SM,
                                                        LangOpts);
        std::string ModifiedBodyCode = BodyCode.str();

        // Проходимо по ReplaceMap та замінюємо змінні
        for (const auto &Entry: ReplaceMap) {
            const std::string &OrigVar = Entry.first;
            const std::string &NewVar = Entry.second;

            // Замінюємо змінні
            std::regex varRegex("\\b" + OrigVar + "\\b");
            ModifiedBodyCode = std::regex_replace(ModifiedBodyCode, varRegex, NewVar);

            // Коригуємо індекси в масивах (OrigVar[i] -> NewVar[i - start])
            std::regex indexRegex(NewVar + "\\s*\\[(.*?)\\]");
            ModifiedBodyCode = std::regex_replace(ModifiedBodyCode, indexRegex, NewVar + "[$1 - start]");
        }

        return " " + ModifiedBodyCode;
    }
};