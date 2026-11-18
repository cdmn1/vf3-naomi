@echo off
setlocal
cd /d "%~dp0"
set "VF3_INTERPRETER=1"
set "VF3_WATCH=%~dp0VF3_WATCH_LOADER.txt"
set "VF3_TRACE=%~dp0vf3-loader-trace-loader.bin"
set "VF3_DIAG=%VF3_TRACE%.diag.txt"
if exist "%VF3_TRACE%" del "%VF3_TRACE%"
if exist "%VF3_DIAG%" del "%VF3_DIAG%"
echo.
echo VF3 LOADER tracing is enabled.
echo Load the Dreamcast version of Virtua Fighter 3 in Flycast.
echo Navigate through character select and confirm into Round 1.
echo Let the game reach the first playable round, then close Flycast normally.
echo Do not use the Naomi probe ROM for this capture.
echo.
echo The trace includes GDFS/MT loader PCs and watched memory ranges.
echo Large trace files are expected.
echo Output:
echo %VF3_TRACE%
echo.
start "" /wait "%~dp0flycast.exe"
if exist "%VF3_TRACE%" (
  echo.
  echo Loader trace created. Size:
  for %%F in ("%VF3_TRACE%") do echo %%~zF bytes
) else (
  echo.
  echo No loader trace file was created. Check that Flycast started correctly.
)
if exist "%VF3_DIAG%" (
  echo.
  echo PC diagnostics created:
  echo %VF3_DIAG%
) else (
  echo.
  echo WARNING: PC diagnostics file was not created.
)
pause
