@echo off

c:\green\tcc\tcc -Wall -O2  -o shredx.exe shredx.c
:: c:\green\tcc\upx --best --lzma shredx.exe -oshredx-min.exe

pause