@echo off
setlocal
cd %~dp0
:: gbuild
go build -o shredx.exe main.go 
c:\green\tcc\upx --best --lzma shredx.exe -oshredx-min.exe
endlocal
pause