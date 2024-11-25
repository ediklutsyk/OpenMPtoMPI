#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Tooling/Core/Replacement.h"
#include "clang/Frontend/CompilerInstance.h"

using namespace clang;
using namespace clang::tooling;

class IncludeMPIHeaderCallback : public PPCallbacks {
public:
    IncludeMPIHeaderCallback(Preprocessor &PP, Replacements &Replaces)
            : PP(PP), Replaces(Replaces), MPIIncluded(false), LastIncludeInMainFile(SourceLocation()) {}

    void InclusionDirective(SourceLocation HashLoc, const Token &IncludeTok,
                            StringRef FileName, bool IsAngled,
                            CharSourceRange FilenameRange,
                            OptionalFileEntryRef File, StringRef SearchPath,
                            StringRef RelativePath, const Module *SuggestedModule,
                            bool ModuleImported,
                            SrcMgr::CharacteristicKind FileType) override {
        SourceManager &SM = PP.getSourceManager();
        if (SM.isInMainFile(HashLoc)) {
            LastIncludeInMainFile = HashLoc;
            if (FileName == "mpi.h") {
                MPIIncluded = true;
            }
        }
    }

    void EndOfMainFile() override {
        SourceManager &SM = PP.getSourceManager();
        std::string Code;
        if (!MPIIncluded) {
            Code = "#include <mpi.h>\n";
        }
        Code += "\nint world_rank;\nint world_size;\n\n";
        SourceLocation StartLoc = SM.getLocForStartOfFile(SM.getMainFileID());
        if (LastIncludeInMainFile.isValid()) {
            StartLoc = SM.translateLineCol(SM.getMainFileID(),
                                            SM.getSpellingLineNumber(LastIncludeInMainFile) + 1, 1);
        }
        auto Err = Replaces.add(Replacement(SM, StartLoc, 0, Code));
        if (Err) {
            llvm::errs() << "Помилка додавання глобальних змінних та/або бібліотек: " << llvm::toString(std::move(Err)) << "\n";
        }
    }

private:
    Preprocessor &PP;
    Replacements &Replaces;
    bool MPIIncluded;
    SourceLocation LastIncludeInMainFile;
};

