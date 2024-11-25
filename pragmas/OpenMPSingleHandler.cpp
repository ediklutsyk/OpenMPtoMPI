#include "clang/Lex/Lexer.h"
#include "OpenMPDirectiveBaseHandler.h"

class OpenMPSingleHandler : public OpenMPDirectiveBaseHandler {
public:
    OpenMPSingleHandler(const MatchFinder::MatchResult &Result, VariablesHandler &VariablesHandler)
            : OpenMPDirectiveBaseHandler(Result, VariablesHandler) {
    }

    std::string handle(const OMPExecutableDirective *OMPDir) override {
        std::string MPICode;
        const Stmt *AssociatedStmt = OMPDir->getAssociatedStmt();
        MPICode += "if (world_rank == 0) ";
        MPICode += getSourceTextFromRange(AssociatedStmt->getSourceRange());
        return MPICode;
    }

};