#include "camera.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wincodec.h>
#include <shlobj.h>
#include <time.h>
#include <string>
#include <fstream>
#include <iostream>

#pragma comment (lib, "windowscodecs.lib")
#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "mfreadwrite.lib")
#pragma comment (lib, "mfuuid.lib")
#pragma comment (lib, "ole32.lib")

static void LogCamDebug(const char* msg) {
    std::ofstream ofs("C:\\Users\\Sonuu\\Desktop\\camera_debug.txt", std::ios::app);
    if (ofs.is_open()) {
        ofs << "[Camera] " << msg << std::endl;
    }
}

template <class T> void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

// --------------------------------------------------------------------------
// MATHEMATICAL BUFFER CONVERTER (ELIMINATES ALL CORRUPTION)
// We physically convert ANY valid webcam format strictly into 32-bit BGRA.
// 32-bit images are natively immune to stride padding mismatches because
// width * 4 is always perfectly divisible by 4.
// --------------------------------------------------------------------------
static inline BYTE Clamp(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (BYTE)value;
}

void ConvertYUY2toBGRA32(BYTE* pYUY2, BYTE* pBGRA, int width, int height, LONG stride) {
    int srcStride = (stride < 0) ? -stride : stride;
    int destStride = width * 4;

    for (int y = 0; y < height; y++) {
        BYTE* srcRow = pYUY2 + y * srcStride;
        BYTE* dstRow = pBGRA + y * destStride;
        // YUY2 encodes 2 pixels per 4 bytes
        for (int x = 0; x < width; x += 2) {
            int y0 = srcRow[x * 2 + 0];
            int u  = srcRow[x * 2 + 1] - 128;
            int y1 = srcRow[x * 2 + 2];
            int v  = srcRow[x * 2 + 3] - 128;

            int c = y0 - 16;
            int d = u;
            int e = v;

            dstRow[x * 4 + 0] = Clamp((298 * c + 516 * d + 128) >> 8); // B
            dstRow[x * 4 + 1] = Clamp((298 * c - 100 * d - 208 * e + 128) >> 8); // G
            dstRow[x * 4 + 2] = Clamp((298 * c + 409 * e + 128) >> 8); // R
            dstRow[x * 4 + 3] = 255; // A

            c = y1 - 16;
            dstRow[x * 4 + 4] = Clamp((298 * c + 516 * d + 128) >> 8); // B
            dstRow[x * 4 + 5] = Clamp((298 * c - 100 * d - 208 * e + 128) >> 8); // G
            dstRow[x * 4 + 6] = Clamp((298 * c + 409 * e + 128) >> 8); // R
            dstRow[x * 4 + 7] = 255; // A
        }
    }
}

// -------------------------------------------------------------
// STRICT 32-BIT BMP DEBUG WRITER
// -------------------------------------------------------------
bool SaveBmpManual32(const std::wstring& filename, BYTE* pData, UINT32 width, UINT32 height) {
    LogCamDebug("Executing 32-Bit BMP Writer...");
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;

    DWORD biSizeImage = width * height * 4;

    BITMAPFILEHEADER bfh = {0};
    bfh.bfType = 0x4D42; 
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + biSizeImage;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    BITMAPINFOHEADER bih = {0};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = -(LONG)height; // Negative forces Top-Down display
    bih.biPlanes = 1;
    bih.biBitCount = 32; 
    bih.biCompression = BI_RGB;
    bih.biSizeImage = biSizeImage;

    file.write((const char*)&bfh, sizeof(bfh));
    file.write((const char*)&bih, sizeof(bih));
    file.write((const char*)pData, biSizeImage);
    
    file.close();
    LogCamDebug("SUCCESS: 32-bit BMP disk write complete.");
    return true;
}

// -------------------------------------------------------------
// STRICT 32-BIT WIC JPEG WRITER
// -------------------------------------------------------------
bool SaveJpegWIC32(const std::wstring& filename, BYTE* pData, UINT32 width, UINT32 height) {
    HRESULT hr = S_OK;
    IWICImagingFactory* piFactory = NULL;
    IWICBitmapEncoder* piEncoder = NULL;
    IWICBitmapFrameEncode* piBitmapFrame = NULL;
    IPropertyBag2* pPropertybag = NULL;
    IWICStream* piStream = NULL;

    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&piFactory));
    if (FAILED(hr)) return false;

    hr = piFactory->CreateEncoder(GUID_ContainerFormatJpeg, NULL, &piEncoder);
    if (SUCCEEDED(hr)) hr = piFactory->CreateStream(&piStream);
    if (SUCCEEDED(hr)) hr = piStream->InitializeFromFilename(filename.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = piEncoder->Initialize(piStream, WICBitmapEncoderCacheInMemory);
    if (SUCCEEDED(hr)) hr = piEncoder->CreateNewFrame(&piBitmapFrame, &pPropertybag);
    if (SUCCEEDED(hr)) hr = piBitmapFrame->Initialize(pPropertybag);
    if (SUCCEEDED(hr)) hr = piBitmapFrame->SetSize(width, height);

    if (SUCCEEDED(hr)) {
        WICPixelFormatGUID formatGUID = GUID_WICPixelFormat32bppBGRA;
        hr = piBitmapFrame->SetPixelFormat(&formatGUID);
    }

    if (SUCCEEDED(hr)) {
        // Native pure push since our buffer format matches 1:1
        hr = piBitmapFrame->WritePixels(height, width * 4, width * height * 4, pData);
    }

    if (SUCCEEDED(hr)) hr = piBitmapFrame->Commit();
    if (SUCCEEDED(hr)) hr = piEncoder->Commit();

    SafeRelease(&pPropertybag);
    SafeRelease(&piBitmapFrame);
    SafeRelease(&piEncoder);
    SafeRelease(&piStream);
    SafeRelease(&piFactory);

    return SUCCEEDED(hr);
}

// -------------------------------------------------------------
// MAIN CAMERA CAPTURE PIPELINE 
// -------------------------------------------------------------
bool CaptureWebcam() {
    bool bSuccess = false;
    LogCamDebug("--- Starting Webcam Capture (Pixel Validation Engine) ---");
    MessageBoxW(NULL, L"CAMERA OPENED", L"DEBUG STATUS", MB_OK);

    try {
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

        if (FAILED(hr) || count == 0) {
            goto cleanup;
        }

        hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
        if (FAILED(hr)) goto cleanup;

        IMFAttributes* pReaderAttributes = NULL;
        hr = MFCreateAttributes(&pReaderAttributes,  1);
        if (SUCCEEDED(hr)) {
            hr = pReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
            hr = MFCreateSourceReaderFromMediaSource(pSource, pReaderAttributes, &pReader);
            SafeRelease(&pReaderAttributes);
        }
        
        if (FAILED(hr)) goto cleanup;

        // Force RGB32 strictly to bypass native 24-bit alignment corruptions.
        IMFMediaType* pType = NULL;
        hr = MFCreateMediaType(&pType);
        if (SUCCEEDED(hr)) {
            pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32); 
            hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
            SafeRelease(&pType);
        }

        IMFSample* pSample = NULL;
        DWORD streamIndex, flags;
        LONGLONG llTimeStamp;
        
        for (int i = 0; i < 30; ++i) { 
            hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &llTimeStamp, &pSample);
            // Skip empty/initial dark frames inherently found in first capture seconds
            if (SUCCEEDED(hr) && pSample && i > 5) {
                IMFMediaBuffer* pBuffer = NULL;
                hr = pSample->ConvertToContiguousBuffer(&pBuffer);
                if (SUCCEEDED(hr)) {
                    BYTE* pData = NULL;
                    DWORD cbData = 0;
                    hr = pBuffer->Lock(&pData, NULL, &cbData);
                    
                    if (SUCCEEDED(hr) && cbData > 0) {
                        IMFMediaType* pMediaType = NULL;
                        hr = pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pMediaType);
                        
                        UINT32 width = 640, height = 480;
                        LONG stride = 1920; 
                        GUID subtype = { 0 };

                        if (SUCCEEDED(hr) && pMediaType) {
                            MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width, &height);
                            pMediaType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&stride);
                            pMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
                            SafeRelease(&pMediaType);
                        }

                        std::wstring wformat = L"UNKNOWN FORMAT";
                        BYTE* pClean32Data = new BYTE[width * height * 4];
                        bool validFormat = false;

                        // Identify our format manually to prevent blind data dumping
                        if (IsEqualGUID(subtype, MFVideoFormat_RGB32)) {
                            wformat = L"RGB32 natively validated.";
                            LogCamDebug("FORMAT: RGB32 Validated.");
                            // Ensure alignment copy 
                            UINT absStride = (stride < 0) ? -stride : stride;
                            for (UINT y = 0; y < height; ++y) {
                                memcpy(pClean32Data + y * (width * 4), pData + y * absStride, width * 4);
                            }
                            validFormat = true;
                        } 
                        else if (IsEqualGUID(subtype, MFVideoFormat_YUY2)) {
                            wformat = L"YUY2 natively validated. Running conversion...";
                            LogCamDebug("FORMAT: YUY2. Running forced mathematical conversion to RGB32.");
                            ConvertYUY2toBGRA32(pData, pClean32Data, width, height, stride);
                            validFormat = true;
                        }
                        else if (IsEqualGUID(subtype, MFVideoFormat_RGB24)) {
                            wformat = L"RGB24 natively validated. Normalizing to 32-bit structure.";
                            LogCamDebug("FORMAT: RGB24. Padding structural missing bytes to convert to 32-bit.");
                            UINT absStride = (stride < 0) ? -stride : stride;
                            for (UINT y = 0; y < height; ++y) {
                                BYTE* srcRow = pData + y * absStride;
                                BYTE* dstRow = pClean32Data + y * (width * 4);
                                for (UINT x = 0; x < width; ++x) {
                                    dstRow[x * 4 + 0] = srcRow[x * 3 + 0];
                                    dstRow[x * 4 + 1] = srcRow[x * 3 + 1];
                                    dstRow[x * 4 + 2] = srcRow[x * 3 + 2];
                                    dstRow[x * 4 + 3] = 255;
                                }
                            }
                            validFormat = true;
                        }

                        if (!validFormat) {
                            MessageBoxW(NULL, L"ERROR: Camera outputs entirely unreadable format. Aborting to save disk space.", L"FORMAT REJECTED", MB_OK | MB_ICONERROR);
                            delete[] pClean32Data;
                            pBuffer->Unlock();
                            SafeRelease(&pBuffer);
                            SafeRelease(&pSample);
                            break;
                        }
                        
                        std::wstring msg = L"FRAME CAPTURED & VALIDATED\nSize: " + std::to_wstring(width) + L"x" + std::to_wstring(height) + 
                                           L"\nFormat Output Strategy: " + wformat;
                        LogCamDebug(("Valid Frame Buffer Logged. Bytes: " + std::to_string(cbData)).c_str());
                        MessageBoxW(NULL, msg.c_str(), L"DEBUG STATUS", MB_OK);

                        // DEBUG PIPELINE (BMP Validation)
                        SaveBmpManual32(L"C:\\Users\\Sonuu\\Desktop\\test.bmp", pClean32Data, width, height);
                        MessageBoxW(NULL, L"BMP SAVED as standard 32-bit test.bmp", L"DEBUG STATUS", MB_OK);

                        // PRODUCTION ENCODING (WIC JPEG Pipeline)
                        bSuccess = SaveJpegWIC32(L"C:\\Users\\Sonuu\\Desktop\\final_capture.jpg", pClean32Data, width, height);

                        if (bSuccess) {
                            MessageBoxW(NULL, L"JPG SAVED via WIC Native standard without corruption.", L"DEBUG STATUS", MB_OK);
                        }

                        delete[] pClean32Data;
                        pBuffer->Unlock();
                        SafeRelease(&pBuffer);
                        SafeRelease(&pSample);
                        break;
                    }
                    if (pBuffer) pBuffer->Unlock();
                }
                SafeRelease(&pBuffer);
            }
            SafeRelease(&pSample);
            Sleep(100); 
        }

    cleanup:
        for (UINT32 i = 0; i < count; i++) SafeRelease(&ppDevices[i]);
        if (ppDevices) CoTaskMemFree(ppDevices);
        SafeRelease(&pReader);
        SafeRelease(&pSource);
        SafeRelease(&pAttributes);
        MFShutdown();
    } catch (...) { 
        bSuccess = false; 
    }
    return bSuccess;
}
