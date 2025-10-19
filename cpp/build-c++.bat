@echo off
echo Building ShredX - Secure File Deletion Tool
echo This is a legitimate system administration utility

setlocal
SET VC=C:\green\VCompiler
SET PATH=%PATH%;%VC%\Bin
SET INCLUDE=%VC%\INCLUDE
SET LIBPATH=%VC%\Lib
SET LIB=%VC%\Lib
rem Try to compile resource file first
echo Compiling resource file...
@echo off
setlocal

echo Building ShredX - Secure File Deletion Tool (MSVC - custom path)
echo This is a legitimate system administration utility

rem === Manually set up PATH for your VC tools ===
set VC_BIN=C:\green\VCompiler\bin
set PATH=%VC_BIN%;%PATH%

rem === Compile resource file ===
echo Compiling resource file...

if exist "%VC_BIN%\rc.exe" (
    rc /fo version.res version.rc
    if %errorlevel% neq 0 (
        echo Warning: version.rc failed to compile, trying version-simple.rc...
        rc /fo version.res version-simple.rc
        if %errorlevel% neq 0 (
            echo Warning: Resource compilation failed, continuing without version info...
            set SKIP_RES=1
        ) else (
            set SKIP_RES=0
        )
    ) else (
        set SKIP_RES=0
    )
) else (
    echo WARNING: rc.exe not found in %VC_BIN%. Skipping version resource.
    set SKIP_RES=1
)

rem === Compile main program ===
echo Compiling shredx.c with cl.exe...
if "%SKIP_RES%"=="1" (
    cl /nologo /W3 /O2 /MD  /EHsc shredx.cpp  /Fe:shredx.exe
) else (
    cl /nologo /W3 /O2 /MD /EHsc shredx.cpp version.res  /Fe:shredx.exe
)
link shredx.obj /OUT:shredx.exe
if %errorlevel% neq 0 (
    echo ERROR: Compilation failed.
    goto :end_fail
)

rem === Optional: Compress with UPX ===
if exist "c:\green\tcc\upx.exe" (
    echo Compressing with UPX...
    "c:\green\tcc\upx.exe" --best --lzma shredx.exe -oshredx-min.exe -f
    if %errorlevel%==0 (
        echo UPX compressed output: shredx-min.exe
    ) else (
        echo UPX compression failed.
    )
) else (
    echo UPX not found at c:\green\tcc\upx.exe - skipping compression.
)

echo Build complete. This tool is for legitimate secure file deletion purposes only.
pause
goto :eof

:end_fail
echo Build aborted.
pause
exit /b 1


endlocal
pause