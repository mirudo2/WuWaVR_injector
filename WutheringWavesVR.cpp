#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>  // Certifique-se de que a biblioteca filesystem foi incluída corretamente

// Certifique-se de que estamos usando o namespace std::filesystem
namespace fs = std::filesystem;

std::vector<DWORD> GetProcessIds(const std::wstring& processName);
bool InjectDLL(DWORD processId, const std::wstring& dllPath);

int main() {
    const std::wstring processName = L"Client-Win64-Shipping.exe";
    const std::vector<std::wstring> dlls = {
        L"openvr_api.dll",
        L"LuaVR.dll",
        L"UEVRBackend.dll"
    };

    // Verificar se o processo "vrserver.exe" está em execução usando GetProcessIds
    if (GetProcessIds(L"vrserver.exe").empty()) {
        MessageBoxW(nullptr, L"SteamVR is not running. Please launch SteamVR first.", L"Warning", MB_OK | MB_ICONWARNING);
        return 1; // Finalizar o programa se o VR Server não estiver em execução
    }

    std::wcout << L"Start Wuthering Waves now ..." << std::endl;

    while (true) {
        std::vector<DWORD> processIds = GetProcessIds(processName);

        if (!processIds.empty()) {
            for (DWORD processId : processIds) {
                for (const std::wstring& dll : dlls) {
                    std::wstring dllPath = fs::current_path().wstring() + L"\\" + dll;

                    // Verificar se a DLL existe
                    if (fs::exists(dllPath)) {
                        if (InjectDLL(processId, dllPath)) {
                            std::wcout << L"Successfully injected " << dll << L" into process " << processName << std::endl;
                        }
                        else {
                            std::wcerr << L"Failed to inject " << dll << L" into process " << processName << std::endl;
                        }
                    }
                    else {
                        std::wcerr << L"Skipping injection, DLL not found: " << dllPath << std::endl;
                    }
                }
            }
            break;  // Saia do loop depois de injetar as DLLs
        }

        Sleep(50);
    }

    return 0;
}

std::vector<DWORD> GetProcessIds(const std::wstring& processName) {
    std::vector<DWORD> processIds;
    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return processIds;
    }

    if (Process32First(snapshot, &processEntry)) {
        do {
            if (processName == processEntry.szExeFile) {
                processIds.push_back(processEntry.th32ProcessID);
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return processIds;
}

bool InjectDLL(DWORD processId, const std::wstring& dllPath) {
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!process) {
        return false;
    }

    LPVOID allocMem = VirtualAllocEx(process, nullptr, dllPath.size() * sizeof(wchar_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!allocMem) {
        CloseHandle(process);
        return false;
    }

    if (!WriteProcessMemory(process, allocMem, dllPath.c_str(), dllPath.size() * sizeof(wchar_t), nullptr)) {
        VirtualFreeEx(process, allocMem, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HANDLE thread = CreateRemoteThread(process, nullptr, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, allocMem, 0, nullptr);
    if (!thread) {
        VirtualFreeEx(process, allocMem, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    WaitForSingleObject(thread, INFINITE);
    VirtualFreeEx(process, allocMem, 0, MEM_RELEASE);
    CloseHandle(thread);
    CloseHandle(process);

    return true;
}
