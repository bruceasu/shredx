@echo off
SetLocal
set CWD=%CD%
cd /d %~dp0
set APP_HOME=%CD%
reg add HKCU\Software\Classes\*\shell\tiny-shred /t REG_EXPAND_SZ /d "快速安全删除" /f
reg add HKCU\Software\Classes\*\shell\tiny-shred\command /t REG_EXPAND_SZ /d "cmd.exe /c %APP_HOME%\tiny-shred.exe -i \"%%1\"" /f
reg add HKCU\Software\Classes\Directory\shell\tiny-shred /t REG_EXPAND_SZ /d "快速安全删除" /f
reg add HKCU\Software\Classes\Directory\shell\tiny-shred\command /t REG_EXPAND_SZ /d "cmd.exe /c %APP_HOME%tiny-shred -i \"%%1\"" /f
reg add HKCU\Software\Classes\Directory\shell\tiny-shred /t REG_EXPAND_SZ /d "快速安全删除(含子目录)" /f
reg add HKCU\Software\Classes\Directory\shell\tiny-shred\command /t REG_EXPAND_SZ /d "cmd.exe /c %APP_HOME%tiny-shred -i -r \"%%1\"" /f
cd %CWD%
Endlocal