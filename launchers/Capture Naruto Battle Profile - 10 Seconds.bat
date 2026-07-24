@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "CAPTURE=%ROOT%\tooling\rexglue-sdk\out\build\tracy-capture-cl\tracy-capture.exe"
set "OUTPUT=%ROOT%\recomp\fase4\traces\narutobb_battle_profile.tracy"

if not exist "%CAPTURE%" (
    echo Tracy capture tool not found:
    echo %CAPTURE%
    pause
    exit /b 1
)

echo Recording 10 seconds. Keep attacking until this window reports completion.
"%CAPTURE%" -o "%OUTPUT%" -s 10 -f
if errorlevel 1 (
    echo Capture failed. Make sure the Tracy profiling build is running.
    pause
    exit /b 1
)

echo Battle profile saved:
echo %OUTPUT%
pause
