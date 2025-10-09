@echo off
reg delete HKCU\Software\Classes\*\shell\shredx\command /f
reg delete HKCU\Software\Classes\*\shell\shredx  /f
reg delete HKCU\Software\Classes\Directory\shell\shredx\command /f
reg delete HKCU\Software\Classes\Directory\shell\shredx /f
