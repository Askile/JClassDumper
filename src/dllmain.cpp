#include <Windows.h>
#include <Config.h>
#include <Dumper.h>
#include <string>
#include "Utils.h"
#include "Logger.h"

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpReserved)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH: {
            std::string baseDir = Utils::GetLocalAppDataPath() + "\\JClassDumper";
            std::string configPath = baseDir + "\\config.json";

            Config config = Config(configPath);

            Dumper::findSignaturesAsync(config.pattern, config.pattern_size, config.header_size);
            break;
        }
    }
    return TRUE;
}