#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Core/Replacement.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "finders/MainFunctionFinder.cpp"
#include "finders/OpenMPPragmaFinder.cpp"
#include "actions/PreprocessorAndASTActionFactory.cpp"
#include "utils/FileUtils.h"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

cl::OptionCategory ToolCategory("OpenMP to MPI Converter Options");
cl::opt<bool> ReplaceArg("replace", cl::desc("Replace content in the original file"), cl::cat(ToolCategory));

int main(int argc, const char **argv) {
    auto expectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
    if (!expectedParser) {
        llvm::errs() << "Помилка парсингу опцій: " << llvm::toString(expectedParser.takeError()) << "\n";
        return 1;
    }
    CommonOptionsParser &optionsParser = expectedParser.get();
    // Створюємо об'єкт Replacements для зберігання всіх змін
    Replacements replace;
    MatchFinder finder;
    OpenMPPragmaFinder transformer(replace);
    MainFunctionFinder mainFunctionHandler(replace);
    finder.addMatcher(functionDecl(hasName("main")).bind("mainFunction"), &mainFunctionHandler);
    finder.addMatcher(ompExecutableDirective().bind("ompDirective"), &transformer);

    // Запускаємо інструмент
    ClangTool Tool(optionsParser.getCompilations(), optionsParser.getSourcePathList());
    llvm::outs() << "Запуск інструменту...\n";
    std::unique_ptr<clang::tooling::FrontendActionFactory> factory = std::make_unique<PreprocessorAndASTActionFactory>(
            finder, replace);
    int toolResult = Tool.run(factory.get());

    // Додаємо MPI_Init та MPI_Finalize в main
    for (const auto &filePath: optionsParser.getSourcePathList()) {
        const auto &code = getFileContent(filePath);
        if (!code) {
            llvm::errs() << "Не вдалося отримати вміст файлу: " << filePath << "\n";
            continue;
        }
        // Застосовуємо всі заміни до файлу
        auto changedCode = tooling::applyAllReplacements(*code, replace);
        if (!changedCode) {
            llvm::errs() << "Помилка застосування замін: " << llvm::toString(changedCode.takeError()) << "\n";
            continue;
        }

        std::string outputPath = filePath;
        if (!ReplaceArg) {
            // Знаходимо позицію останнього символу перед розширенням файлу
            size_t dotPos = outputPath.find_last_of('.');
            if (dotPos != std::string::npos) {
                // Вставляємо "-converted" перед розширенням
                outputPath.insert(dotPos, "-converted");
            } else {
                // Якщо розширення немає, просто додаємо "-converted.cpp"
                outputPath += "-converted.cpp";
            }
        }

        if (!writeFileContent(outputPath, *changedCode)) {
            llvm::errs() << "Помилка запису зміненого коду у файл: " << outputPath << "\n";
        } else {
            llvm::outs() << "Зміни успішно записані у файл: " << outputPath << "\n";
        }

        // Форматуємо файл за допомогою clang-format
        std::string command = "clang-format -i " + outputPath;
        int result = std::system(command.c_str());

        if (result != 0) {
            llvm::errs() << "Не вдалося виконати clang-format для файлу: " << outputPath << "\n";
        } else {
            llvm::outs() << "Форматування файлу успішно завершено: " << outputPath << "\n";
        }
    }

    return toolResult;
}
