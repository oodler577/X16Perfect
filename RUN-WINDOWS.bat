@echo off
setlocal
pushd "%~dp0x16perfect" || goto missing
if not exist "X16PERF.PRG" goto incomplete
if not exist "%~dp0x16emu-r49\x16emu.exe" goto emulator_missing
if not exist "%~dp0x16emu-r49\rom.bin" goto emulator_missing
"%~dp0x16emu-r49\x16emu.exe" -rom "%~dp0x16emu-r49\rom.bin" -c02 -mhz 8 -ram 512 -scale 2 -fsroot "." -prg "X16PERF.PRG" -run
set "result=%ERRORLEVEL%"
popd
if not "%result%"=="0" (
    echo X16Emu exited with code %result%.
    pause
)
exit /b %result%

:incomplete
popd
:missing
echo Extract the entire ZIP to a folder, then double-click RUN-WINDOWS.bat.
pause
exit /b 1

:emulator_missing
popd
echo Download X16Emu R49 for Windows from:
echo https://github.com/X16Community/x16-emulator/releases/tag/r49
echo Extract it into the x16emu-r49 folder next to this launcher.
pause
exit /b 1
