#include "Config.h"
#include "Logger.h"
#include <json.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

using json = nlohmann::json;

static BYTE hexToByte(const std::string& hex)
{
    return static_cast<BYTE>(std::stoul(hex, nullptr, 16));
}

Config::Config(std::string path) {
    pattern = nullptr;
    pattern_size = 0;
    header_size = 0;

    std::filesystem::path filePath(path);
    std::filesystem::path dirPath = filePath.parent_path();

    if (!dirPath.empty()) {
        std::filesystem::create_directories(dirPath);
    }

    std::ifstream file(path);
    if (file.is_open()) {
        json j;
        file >> j;

        if (j.contains("pattern")) {
            std::string hexStr = j["pattern"].get<std::string>();
            std::istringstream iss(hexStr);
            std::string byteStr;
            while (iss >> byteStr) {
                pattern_storage.push_back(hexToByte(byteStr));
            }
            pattern = pattern_storage.data();
            pattern_size = pattern_storage.size();
        }

        if (j.contains("header_size")) {
            header_size = j["header_size"].get<int>();
        }
    }
    else {
        json j;
        j["pattern"] = "CA FE BA BE";
        j["header_size"] = 4;

        std::ofstream outFile(path);
        if (outFile.is_open()) {
            outFile << j.dump(4);
            outFile.close();

            pattern_storage = { 0xCA, 0xFE, 0xBA, 0xBE };
            pattern = pattern_storage.data();
            pattern_size = pattern_storage.size();
        }
    }
    file.close();
}