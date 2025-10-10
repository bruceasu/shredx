@echo off
setlocal
cd %~dp0
:: gbuild
D:\green\go\bin\go build -o shredx.exe main.go 
endlocal
pause