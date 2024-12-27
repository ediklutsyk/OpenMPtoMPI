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
            // Отримуємо MPI тип змінної
            std::string mpiType = getMPITypeFromVariableType(varInfo.type);
            if (mpiType.empty()) {
                dataTransferCode += "/* Неможливо визначити MPI тип для змінної " + varName + " */\n";
                continue;
            }
            if (varInfo.isLocal) {
                continue; // Ігноруємо локальні змінні
            }
            if (varInfo.type->isArrayType() || varInfo.type->isPointerType()) {
                std::string arrayBaseType = varInfo.type->getAsArrayTypeUnsafe()->getElementType().getAsString();
                if (varInfo.isRead && !varInfo.isWritten) {
                    // Масив використовується для читання
                    dataTransferCode += "// Розподіл масиву " + varName + " між процесами\n";
                    std::string LocalVarDecl = Variables.useOrInitVariable("local_" + varName, arrayBaseType);
                    dataTransferCode += LocalVarDecl + " [chunk];\n";
                    dataTransferCode += "MPI_Scatter(" + varName + ", chunk, " + mpiType + ", local_" + varName +
                                        ", chunk, " + mpiType + ", 0, MPI_COMM_WORLD);\n";
                    ReplaceMap[varName] = "local_" + varName;
                } else if (!varInfo.isRead && varInfo.isWritten) {
                    // Масив використовується для запису
                    dataTransferCode +=
                            "// Підготовка локального буфера для запису результатів у масив " + varName + "\n";
                    std::string LocalVarDecl = Variables.useOrInitVariable("local_" + varName, arrayBaseType);
                    dataTransferCode += LocalVarDecl + " [chunk];\n";
                    ReplaceMap[varName] = "local_" + varName;
                    afterLoopCode += "MPI_Gather(" + ReplaceMap[varName] + ", chunk, " + mpiType + ", " + varName +
                                     ", chunk, " + mpiType + ", 0, MPI_COMM_WORLD);\n";
                }
            } else if (varInfo.type->isBuiltinType()) {
                if (varInfo.isRead && !varInfo.isWritten) {
                    dataTransferCode += "// Скалярна змінна " + varName + " використовується для читання\n";
                    dataTransferCode += "MPI_Bcast(&" + varName + ", 1, " + mpiType + ", 0, MPI_COMM_WORLD);\n";
                }
//                else if (varInfo.isWritten) {
//                    dataTransferCode += "/* Скалярна змінна " + varName + " модифікується в циклі.\n";
//                    dataTransferCode += "   Розгляньте можливість використання MPI_Reduce для збору результатів. */\n";
//                }
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
        if (!dataTransferCode.empty()) {
            MPICode += dataTransferCode + "\n";
        }
        if (!beforeLoopCode.empty()) {
            MPICode += beforeLoopCode + "\n";
        }
        if (hasReduction) {
            std::string LocalReductionVarDecl = Variables.useOrInitVariable("local_" + ReductionVar, "int");
            if (ReductionOp == "MPI_PROD") {
                MPICode += LocalReductionVarDecl + " = 1;\n";
            } else{
                MPICode += LocalReductionVarDecl + " = 0;\n";
            }
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
            std::string LoopBodyCode = generateLoopCodeWithReplacements(For->getBody(), ReplaceMap, SM, LangOpts);
            MPICode += "for (int i = start; i < end; ++i)" + LoopBodyCode + "\n";
        }
        if (!afterLoopCode.empty()) {
            MPICode += afterLoopCode + "\n";
        }
        return MPICode;
    }

    std::string directiveName() override {
        return "#pragma omp parallel for";
    }

private:

    std::string getMPITypeFromVariableType(const clang::QualType &type) {
        if (type->isBuiltinType()) {
            if (type->isIntegerType()) {
                return "MPI_INT";
            } else if (type->isFloatingType()) {
                return "MPI_FLOAT";
            } else if (type->isDoubleType()) {
                return "MPI_DOUBLE";
            } else if (type->isCharType()) {
                return "MPI_CHAR";
            } else if (type->isBooleanType()) {
                return "MPI_C_BOOL";
            } else {
                llvm::errs() << "Unsupported built-in type for MPI conversion: " << type.getAsString() << "\n";
                return "";
            }
        } else if (type->isPointerType() || type->isArrayType()) {
            const clang::Type *elementType = type->getPointeeOrArrayElementType();
            if (!elementType) {
                llvm::errs() << "Failed to retrieve element type for: " << type.getAsString() << "\n";
                return "";
            }
            // Перетворюємо Type у QualType для подальшої обробки
            clang::QualType qualElementType = elementType->getCanonicalTypeInternal();
            return getMPITypeFromVariableType(qualElementType);
        } else {
            llvm::errs() << "Unsupported type for MPI conversion: " << type.getAsString() << "\n";
            return "";
        }
    }

    std::string generateLoopCodeWithReplacements(const Stmt *Body, const std::map<std::string, std::string> &ReplaceMap,
                                                 const SourceManager &SM, const LangOptions &LangOpts) {
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