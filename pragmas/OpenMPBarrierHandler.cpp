#include "clang/Lex/Lexer.h"
#include "OpenMPDirectiveBaseHandler.h"

class OpenMPBarrierHandler : public OpenMPDirectiveBaseHandler {
public:
    OpenMPBarrierHandler(const MatchFinder::MatchResult &Result, VariablesHandler &VariablesHandler)
            : OpenMPDirectiveBaseHandler(Result, VariablesHandler) {
    }

    std::string handle(const OMPExecutableDirective *OMPDir) override {
        return "MPI_Barrier(MPI_COMM_WORLD);\n";
    }

    std::string directiveName() override {
        return "#pragma omp barrier";
    }

};