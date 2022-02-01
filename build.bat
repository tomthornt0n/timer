@echo off

if not exist build mkdir build
pushd build
rc /nologo /fo .\res.res ..\source\res.rc
cl /nologo /Os /FC /DUNICODE ..\source\main.c /link /nologo /subsystem:windows /release User32.lib Gdi32.lib Winmm.lib Shlwapi.lib res.res /out:timer.exe
popd