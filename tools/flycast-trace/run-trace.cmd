@echo off
setlocal
cd /d "%~dp0"
set "VF3_INTERPRETER=1"
set "VF3_WATCH=%~dp0VF3_WATCH_LOADER.txt"
set "VF3_TRACE=%~dp0vf3-loader-trace.bin"
if exist "%VF3_TRACE%" del "%VF3_TRACE%"
echo.
echo VF3 tracing is enabled.
echo Load the Dreamcast version of Virtua Fighter 3 in Flycast.
echo Use the controls to navigate as far as possible, ideally into a player-controlled match.
echo Then close Flycast normally.
echo The trace will be saved as:
echo %VF3_TRACE%
echo.
start "" /wait "%~dp0flycast.exe"
if exist "%VF3_TRACE%" (
  echo.
  echo Trace created. Size:
  for %%F in ("%VF3_TRACE%") do echo %%~zF bytes
) else (
  echo.
  echo No trace file was created. Check that Flycast started correctly.
)
pause
