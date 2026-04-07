#include "camera.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wincodec.h>
#include <shlobj.h>
#include <time.h>
#include <string>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

template <class T> void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

bool SaveBitmap(const std::wstring& filename, BYTE* pData, DWORD cbData, UINT32 width, UINT32 height) {
    HANDLE hFile = CreateFileW(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    BITMAPFILEHEADER bfh = {0};
    bfh.bfType = 0x4D42; // "BM"
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + cbData;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    BITMAPINFOHEADER bih = {0};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = -(LONG)height; // negative height for top-down
    bih.biPlanes = 1;
    bih.biBitCount = (WORD)((cbData * 8) / (width * height));
    if (bih.biBitCount == 0) bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    DWORD bytesWritten;
    WriteFile(hFile, &bfh, sizeof(BITMAPFILEHEADER), &bytesWritten, NULL);
    WriteFile(hFile, &bih, sizeof(BITMAPINFOHEADER), &bytesWritten, NULL);
    WriteFile(hFile, pData, cbData, &bytesWritten, NULL);
    CloseHandle(hFile);
    return true;
}

bool CaptureWebcam() {
    try {
        // CoInitializeEx is now handled by main.cpp globally
        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) {
            return false;
        }

        IMFAttributes* pAttributes = NULL;
        IMFActivate** ppDevices = NULL;
        UINT32 count = 0;
        IMFMediaSource* pSource = NULL;
        IMFSourceReader* pReader = NULL;
        bool bSuccess = false;

        hr = MFCreateAttributes(&pAttributes, 1);
        if (SUCCEEDED(hr)) {
            hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        }

        if (SUCCEEDED(hr)) {
            hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
        }

        if (SUCCEEDED(hr) && count > 0) {
            hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
            
            if (SUCCEEDED(hr)) {
                hr = MFCreateSourceReaderFromMediaSource(pSource, NULL, &pReader);
            }

            if (SUCCEEDED(hr)) {
                IMFSample* pSample = NULL;
                DWORD streamIndex, flags;
                LONGLONG llTimeStamp;
                
                for (int i = 0; i < 10; ++i) { 
                    hr = pReader->ReadSample(
                        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                        0,
                        &streamIndex,
                        &flags,
                        &llTimeStamp,
                        &pSample
                    );

                    if (SUCCEEDED(hr) && pSample) {
                        IMFMediaBuffer* pBuffer = NULL;
                        hr = pSample->ConvertToContiguousBuffer(&pBuffer);
                        if (SUCCEEDED(hr)) {
                            BYTE* pData = NULL;
                            DWORD cbData = 0;
                            hr = pBuffer->Lock(&pData, NULL, &cbData);
                            if (SUCCEEDED(hr)) {
                                IMFMediaType* pMediaType = NULL;
                                hr = pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pMediaType);
                                UINT32 width = 640;
                                UINT32 height = 480;
                                if (SUCCEEDED(hr) && pMediaType) {
                                    MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width, &height);
                                    SafeRelease(&pMediaType);
                                }

                                time_t now = time(0);
                                struct tm tstruct;
                                localtime_s(&tstruct, &now);
                                char timeBuf[80];
                                strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", &tstruct);

                                WCHAR localAppData[MAX_PATH];
                                if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
                                    std::wstring filePath = std::wstring(localAppData) + L"\\IMG_" + std::wstring(timeBuf, timeBuf + strlen(timeBuf)) + L".bmp"; 
                                    
                                    bSuccess = SaveBitmap(filePath, pData, cbData, width, height);
                                }
                                pBuffer->Unlock();
                            }
                            SafeRelease(&pBuffer);
                        }
                        SafeRelease(&pSample);
                        break;
                    }
                    Sleep(100); 
                }
            }
        }

        for (UINT32 i = 0; i < count; i++) {
            SafeRelease(&ppDevices[i]);
        }
        if (ppDevices) {
            CoTaskMemFree(ppDevices);
        }
        SafeRelease(&pReader);
        SafeRelease(&pSource);
        SafeRelease(&pAttributes);

        MFShutdown();
        return bSuccess;
    } catch (...) {
        return false;
    }
}
