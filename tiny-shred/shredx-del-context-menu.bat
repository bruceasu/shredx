@echo off
reg delete HKCU\Software\Classes\*\shell\tiny-shred\command /f
reg delete HKCU\Software\Classes\*\shell\tiny-shred  /f
reg delete HKCU\Software\Classes\Directory\shell\tiny-shred\command /f
reg delete HKCU\Software\Classes\Directory\shell\tiny-shred /f
