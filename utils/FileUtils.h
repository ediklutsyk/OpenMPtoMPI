#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>
#include <fstream>
#include <optional>
#include "llvm/Support/raw_ostream.h"

// Функція для отримання вмісту файлу
std::optional<std::string> getFileContent(const std::string &FilePath);

// Функція для запису вмісту у файл
bool writeFileContent(const std::string &FilePath, const std::string &Content);

#endif // FILE_UTILS_H

