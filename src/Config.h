#pragma once
#include <string>
#include <vector>
#include <Windows.h>

class Config
{
public:
    Config(std::string path);

    const BYTE* pattern;
    int header_size;
    size_t pattern_size;
private:
    std::vector<BYTE> pattern_storage;
};