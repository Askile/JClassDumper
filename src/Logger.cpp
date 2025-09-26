#include "Logger.h"
#include "Utils.h"
#include <Windows.h>
#include <fstream>
#include <filesystem>
#include <string>

const std::string path = Utils::GetLocalAppDataPath() + "\\JClassDumper\\logs";

void Logger::log(const std::string& message) {
    std::filesystem::create_directories(path);
    std::ofstream file(path + "\\log.txt", std::ios::app);
    if (file.is_open()) {
        file << message << std::endl;
    }
}