@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "CAPTURE=%ROOT%\tooling\rexglue-sdk\out\build\tracy-capture-cl\tracy-capture.exe"
set "OUTPUT=%ROOT%\recomp\fase4\traces\narutobb_battle_control.tracy"
set "STATUS=%ROOT%\tracy_control.status"
set "LOG=%ROOT%\tracy_control.log"

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
echo BATTLE CONTROL CAPTURE AT 60 FPS - 40 SECONDS
echo.
echo Negative control. Same battle, same characters, same stage, and the
echo 60 FPS experiment active exactly like the cut-in capture.
echo.
echo During the 40 seconds:
echo   - Fight normally: attack, block, dodge, get hit, throw shuriken.
echo   - The messier this capture is, the better the comparison works.
echo   - Do NOT activate any status jutsu or portrait overlay.
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
echo Control profile saved:
echo %OUTPUT%
pause
exit /b 0
