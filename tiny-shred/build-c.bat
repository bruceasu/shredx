@echo off
echo Building ShredX - Secure File Deletion Tool
echo This is a legitimate system administration utility

rem Try to compile resource file first
echo Compiling resource file...
set PATH=C:\green\msys64\ucrt64\bin\;%PATH%
C:\green\msys64\ucrt64\bin\windres.exe --input version.rc --output version.o --output-format=coff
if %errorlevel% neq 0 (
    echo Warning: Resource compilation failed, trying simple version...
    D:\green\msys64\ucrt64\bin\windres.exe --input version-simple.rc --output version.o --output-format=coff
    if %errorlevel% neq 0 (
        echo Warning: All resource compilation attempts failed, building without version info
        echo Compiling main program without version resources...
        c:\green\tcc\tcc -Wall -O2 -o tiny-shred.exe tiny-shred.c
        goto :done
    )
)
echo Resource compilation successful
rem Compile main program with version resources
echo Compiling main program with version resources...
:: c:\green\tcc\x86_64-win32-tcc -Wall -O2 -o shredx.exe shredx.c version.o
c:\green\tcc\tcc -m64 -Wall -O2 -o tiny-shred.exe tiny-shred.c version.o
:done

rem Optional: compress with UPX (may increase false positives)
c:\green\tcc\upx --best --lzma tiny-shred.exe -oshredx-min.exe -f

echo Build complete. This tool is for legitimate secure file deletion purposes only.
pause