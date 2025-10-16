@echo off
echo Building ShredX Go Version - Secure File Deletion Tool
echo This is a legitimate system administration utility

setlocal
cd %~dp0

echo Updating dependencies...
D:\green\go\bin\go mod tidy

echo Building Go executable...
D:\green\go\bin\go build -o shredx.exe main.go 

if %errorlevel% neq 0 (
    echo Build failed!
    goto :end
)

echo Build successful!

rem Optional: compress with UPX (may increase false positives with antivirus)
rem c:\green\tcc\upx --best --lzma shredx.exe -oshredx-min.exe -f

echo Build complete. This tool is for legitimate secure file deletion purposes only.

:end
endlocal
pause