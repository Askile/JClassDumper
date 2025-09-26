#include "Utils.h"
#include <Windows.h>
#include <shlobj.h>

std::string Utils::WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::string Utils::GetLocalAppDataPath() {
    PWSTR path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path)))
    {
        std::wstring wpath(path);
        CoTaskMemFree(path);
        return WideToUtf8(wpath);
    }
    return "";
}