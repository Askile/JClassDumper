#pragma once
#include <string>
class Utils
{
public:
	static std::string GetLocalAppDataPath();
	static std::string WideToUtf8(const std::wstring& wstr);
};