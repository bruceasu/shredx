@echo off
echo Building ShredX with TCC only (no external windres)
echo This is a legitimate system administration utility

rem Try TCC's built-in resource compiler first
echo Trying TCC built-in resource compilation...
c:\green\tcc\tcc -Wall -O2 -o shredx.exe shredx.c version.rc
if %errorlevel% neq 0 (
    echo TCC resource compilation failed, building without resources
    c:\green\tcc\tcc -Wall -O2 -o shredx.exe shredx.c
) else (
    echo Build successful with resources
)

echo Build complete. This tool is for legitimate secure file deletion purposes only.
pause