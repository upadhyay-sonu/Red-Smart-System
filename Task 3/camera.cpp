#include "camera.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wincodec.h>
#include <shlobj.h>
#include <time.h>
#include <string>
#include <gdiplus.h>

using namespace Gdiplus;

template <class T> void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

bool SaveJpeg(const std::wstring& filename, BYTE* pData, DWORD cbData, UINT32 width, UINT32 height) {
    CLSID encoderClsid;
    if (GetEncoderClsid(L"image/jpeg", &encoderClsid) < 0) return false;

    int stride = ((width * 3 + 3) & ~3); 
    Bitmap bitmap(width, height, stride, PixelFormat24bppRGB, pData);
    bitmap.RotateFlip(RotateNoneFlipY);

    Status stat = bitmap.Save(filename.c_str(), &encoderClsid, NULL);

    if (stat == Ok) {
        MessageBoxW(NULL, L"JPEG SAVED IN MASTER FOLDER", L"Status", MB_OK);
        return true;
    } else {
        std::wstring msg = L"Gdiplus::Status Error Code: " + std::to_wstring((int)stat);
        MessageBoxW(NULL, msg.c_str(), L"Debug", MB_OK);
        return false;
    }
}

bool CaptureWebcam() {
    bool bSuccess = false;
    try {
        MFShutdown();
        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) return false;

        IMFAttributes* pAttributes = NULL;
        IMFActivate** ppDevices = NULL;
        UINT32 count = 0;
        IMFMediaSource* pSource = NULL;
        IMFSourceReader* pReader = NULL;

        hr = MFCreateAttributes(&pAttributes, 1);
        if (SUCCEEDED(hr)) hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        if (SUCCEEDED(hr)) hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);

        if (SUCCEEDED(hr) && count > 0) {
            hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
            if (SUCCEEDED(hr)) hr = MFCreateSourceReaderFromMediaSource(pSource, NULL, &pReader);

            if (SUCCEEDED(hr)) {
                IMFSample* pSample = NULL;
                DWORD streamIndex, flags;
                LONGLONG llTimeStamp;
                
                for (int i = 0; i < 10; ++i) { 
                    hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &llTimeStamp, &pSample);
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
                                UINT32 width = 640, height = 480;
                                if (SUCCEEDED(hr) && pMediaType) {
                                    MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width, &height);
                                    SafeRelease(&pMediaType);
                                }
                                
                                bSuccess = SaveJpeg(L"final_capture.jpg", pData, cbData, width, height);
                                if (bSuccess) {
                                    MessageBoxW(NULL, L"IMAGE CAPTURED", L"Task 3", MB_OK);
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
        for (UINT32 i = 0; i < count; i++) SafeRelease(&ppDevices[i]);
        if (ppDevices) CoTaskMemFree(ppDevices);
        SafeRelease(&pReader);
        SafeRelease(&pSource);
        SafeRelease(&pAttributes);
        MFShutdown();
    } catch (...) { bSuccess = false; }
    return bSuccess;
}
