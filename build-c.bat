@echo off
setlocal ENABLEDELAYEDEXPANSION
title ShredX Build System
chcp 65001 >nul
rem ==============================================
rem   ShredX - Secure File Deletion Tool
rem   Legitimate System Administration Utility
rem ==============================================

set "MSYS64_ROOT=D:\green\msys64"
set "TCC_ROOT=C:\green\tcc"
set "WINDRES=%MSYS64_ROOT%\ucrt64\bin\windres.exe"
:: set "TCC=%TCC_ROOT%\tcc.exe"
:: set "UPX=%TCC_ROOT%\upx.exe"
set "TCC=%TCC_ROOT%\x86_64-win32-tcc.exe"
set "UPX_EXE=%TCC_ROOT%\x86_64-upx.exe"

rem --- detect mode ---
set "MODE=%~1"
if "%MODE%"=="" set "MODE=build"

echo ==============================================
echo   ShredX Build System
echo   Mode: %MODE%
echo ==============================================
echo.

rem --- mode dispatch ---
if /I "%MODE%"=="build"   goto :BUILD
if /I "%MODE%"=="release" goto :RELEASE
if /I "%MODE%"=="clean"   goto :CLEAN
if /I "%MODE%"=="check"   goto :CHECK

echo ❌ Unknown mode: %MODE%
echo Usage: build [build|release|clean|check]
goto :END


:CHECK
echo 🔍 Checking toolchain...
if exist "%WINDRES%" (
    echo ✅ windres: "%WINDRES%"
    "%WINDRES%" --version | find "windres"
) else (
    echo ❌ windres not found at "%WINDRES%"
)
if exist "%TCC%" (
    echo ✅ tcc: "%TCC%"
    "%TCC%" -v
) else (
    echo ❌ tcc not found at "%TCC%"
)
if exist "%UPX_EXE%" (
    echo ✅ upx: "%UPX_EXE%"
    "%UPX_EXE%" --version
) else (
    echo ⚠️  upx not found (optional)
)
goto :END


:CLEAN
echo 🧹 Cleaning build artifacts...
del /f /q shredx.exe shredx-min.exe version.o 2>nul
echo ✅ Clean complete.
goto :END


:BUILD
echo 🚧 Building ShredX (debug/standard mode)
call :COMPILE
goto :END


:RELEASE
echo 🚀 Building ShredX (release mode, with UPX)
set "EXTRA_FLAGS=-O2"
call :COMPILE
if exist "%UPX_EXE%" (
    echo [3/3]Compressing executable...
    echo "%UPX_EXE%" --best --lzma shredx.exe -oshredx-min.exe
    "%UPX_EXE%" --best --lzma shredx.exe -oshredx-min.exe
    echo ✅ UPX compression complete.
) else (
    echo ⚠️  upx not found, skipping compression.
)
goto :END


:COMPILE
if not exist "%WINDRES%" (
    echo ❌ ERROR: windres.exe not found at "%WINDRES%"
    pause
    exit /b 1
)
if not exist "%TCC%" (
    echo ❌ ERROR: tcc.exe not found at "%TCC%"
    pause
    exit /b 1
)

echo [1/3] Compiling version resources...
"%WINDRES%" --input version.rc --output version.o --output-format=coff
if %errorlevel% neq 0 (
    echo ⚠️  version.rc failed, trying version-simple.rc...
    "%WINDRES%" --input version-simple.rc --output version.o --output-format=coff
    if %errorlevel% neq 0 (
        echo ⚠️  All resource compilation attempts failed.
        echo [Fallback] Building without resources...
        "%TCC%" -Wall %EXTRA_FLAGS% -o shredx.exe shredx.c
        goto :EOF
    )
)
echo ✅ Resource compiled.

echo [2/3] Compiling main program...
"%TCC%" -Wall %EXTRA_FLAGS% -o shredx.exe shredx.c version.o
if %errorlevel% neq 0 (
    echo ❌ Compilation failed.
    pause
    exit /b 1
)
echo ✅ Build succeeded.

goto :EOF


:END
echo.
echo ==============================================
echo ✅ Operation complete.
echo This tool is for legitimate secure file deletion purposes only.
echo ==============================================
pause
endlocal
chcp 936 >nul