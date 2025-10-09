@echo off
SetLocal
set CWD=%CD%
cd /d %~dp0
set APP_HOME=%CD%
reg add HKCU\Software\Classes\*\shell\shredx /t REG_EXPAND_SZ /d "强力删除" /f
reg add HKCU\Software\Classes\*\shell\shredx\command /t REG_EXPAND_SZ /d "cmd.exe /c %APP_HOME%\shredx.exe -f -s \"%%1\"" /f
reg add HKCU\Software\Classes\Directory\shell\shredx /t REG_EXPAND_SZ /d "强力删除" /f
reg add HKCU\Software\Classes\Directory\shell\shredx\command /t REG_EXPAND_SZ /d "cmd.exe /c %APP_HOME%\shredx.exe -f -s \"%%1\"" /f
cd %CWD%
Endlocal