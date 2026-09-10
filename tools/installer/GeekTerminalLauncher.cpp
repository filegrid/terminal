#include <windows.h>
#include <shellapi.h>

#include <string>

namespace
{
    std::wstring QuoteArgument(const std::wstring& value)
    {
        std::wstring quoted{ L"\"" };
        size_t slashes = 0;
        for (const auto character : value)
        {
            if (character == L'\\')
            {
                ++slashes;
            }
            else if (character == L'\"')
            {
                quoted.append(slashes * 2 + 1, L'\\');
                quoted.push_back(character);
                slashes = 0;
            }
            else
            {
                quoted.append(slashes, L'\\');
                quoted.push_back(character);
                slashes = 0;
            }
        }
        quoted.append(slashes * 2, L'\\');
        quoted.push_back(L'\"');
        return quoted;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t launcherPath[MAX_PATH];
    const auto length = GetModuleFileNameW(nullptr, launcherPath, ARRAYSIZE(launcherPath));
    if (!length || length == ARRAYSIZE(launcherPath))
    {
        return ERROR_PATH_NOT_FOUND;
    }

    std::wstring installRoot{ launcherPath, length };
    installRoot.erase(installRoot.find_last_of(L"\\/") + 1);
    const auto runtimeDirectory = installRoot + L"bin\\";
    const auto runtimeExecutable = runtimeDirectory + L"GeekTerminal.exe";

    int argumentCount = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    std::wstring commandLine = QuoteArgument(runtimeExecutable);
    for (int index = 1; arguments && index < argumentCount; ++index)
    {
        commandLine += L" ";
        commandLine += QuoteArgument(arguments[index]);
    }
    LocalFree(arguments);

    STARTUPINFOW startupInfo{ .cb = sizeof(startupInfo) };
    PROCESS_INFORMATION processInfo{};
    if (!CreateProcessW(runtimeExecutable.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, runtimeDirectory.c_str(), &startupInfo, &processInfo))
    {
        return static_cast<int>(GetLastError());
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return ERROR_SUCCESS;
}
