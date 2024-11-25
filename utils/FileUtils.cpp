#include "FileUtils.h"

// Функція для отримання вмісту файлу
std::optional<std::string> getFileContent(const std::string &FilePath) {
    std::ifstream inFile(FilePath);
    if (!inFile) {
        llvm::errs() << "Помилка відкриття файлу: " << FilePath << "\n";
        return std::nullopt;
    }
    std::string content((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    return content;
}

// Функція для запису вмісту у файл
bool writeFileContent(const std::string &FilePath, const std::string &Content) {
    std::ofstream outFile(FilePath);
    if (!outFile) {
        llvm::errs() << "Не вдалося відкрити файл для запису: " << FilePath << "\n";
        return false;
    }
    outFile << Content;
    return true;
}
