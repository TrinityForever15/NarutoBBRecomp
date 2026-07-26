@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "CAPTURE=%ROOT%\tooling\rexglue-sdk\out\build\tracy-capture-cl\tracy-capture.exe"
set "OUTPUT=%ROOT%\recomp\fase4\traces\narutobb_cutin_jutsu_30fps.tracy"
set "STATUS=%ROOT%\tracy_cutin30.status"
set "LOG=%ROOT%\tracy_cutin30.log"

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
echo CUT-IN JUTSU CAPTURE AT ORIGINAL 30 FPS - 40 SECONDS
echo.
echo This is the reference capture. The 60 FPS experiment must be OFF:
echo do NOT press F8, or press F8 again to suspend it before capturing.
echo.
echo Use the same battle, same character and the same status jutsu as the
echo 60 FPS capture, and try to activate it a similar number of times.
echo.
echo During the 40 seconds:
echo   - Activate the STATUS jutsu that shows your character portrait.
echo   - Repeat it as many times as chakra allows.
echo   - Let each portrait overlay play out completely.
echo   - Do NOT use the other jutsu types.
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
echo 30 FPS reference cut-in profile saved:
echo %OUTPUT%
pause
exit /b 0
