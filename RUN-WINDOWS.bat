@echo off
setlocal
cd /d "%~dp0" || exit /b 1
if not exist "..\x16emu.exe" goto emulator
if not exist "..\rom.bin" goto emulator
for %%F in (X16PERF.PRG) do if not exist "%%F" goto missing
"..\x16emu.exe" -rom "..\rom.bin" -c02 -mhz 8 -ram 512 -fsroot . -prg X16PERF.PRG -run -scale 2
set "RUN_RESULT=%errorlevel%"
if not "%RUN_RESULT%"=="0" pause
exit /b %RUN_RESULT%
:missing
echo A required X16Perfect file is missing. Extract the complete runtime ZIP again.
pause
exit /b 1
:emulator
echo Place x16emu.exe and its matching rom.bin in the parent folder.
pause
exit /b 1
