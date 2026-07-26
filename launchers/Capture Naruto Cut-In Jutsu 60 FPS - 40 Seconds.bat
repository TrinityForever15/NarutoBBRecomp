@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "CAPTURE=%ROOT%\tooling\rexglue-sdk\out\build\tracy-capture-cl\tracy-capture.exe"
set "OUTPUT=%ROOT%\recomp\fase4\traces\narutobb_cutin_jutsu.tracy"
set "STATUS=%ROOT%\tracy_cutin.status"
set "LOG=%ROOT%\tracy_cutin.log"

echo RUNNING> "%STATUS%"

if not exist "%CAPTURE%" (
    echo Tracy capture tool not found: %CAPTURE%> "%LOG%"
    echo FAIL> "%STATUS%"
    echo Tracy capture tool not found:
    echo %CAPTURE%
    pause
    exit /b 1
)

echo.
echo CUT-IN JUTSU CAPTURE AT 60 FPS - 40 SECONDS
echo.
echo Requirements before starting:
echo   - The Tracy 60 FPS build is running.
echo   - F8 was pressed once, so the 60 FPS experiment is active.
echo   - You are already inside a battle.
echo.
echo During the 40 seconds:
echo   - Activate the STATUS jutsu that shows your character portrait.
echo   - Repeat it as many times as chakra allows. More activations is better.
echo   - Let each portrait overlay play out completely.
echo   - Do NOT use the other jutsu types.
echo.
echo Taking hits is fine. Normal combat noise cancels out against the
echo control capture. Only the status jutsu must be exclusive to this run.
echo.
pause

"%CAPTURE%" -o "%OUTPUT%" -s 40 -f > "%LOG%" 2>&1
if errorlevel 1 (
    echo FAIL> "%STATUS%"
    echo Capture failed. Make sure the Tracy profiling build is running.
    type "%LOG%"
    pause
    exit /b 1
)

echo OK> "%STATUS%"
echo.
echo Cut-in profile saved:
echo %OUTPUT%
pause
exit /b 0
