@echo off
setlocal enabledelayedexpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "EXPORT=%ROOT%\tooling\rexglue-sdk\out\build\tracy-csvexport-cl\tracy-csvexport.exe"
set "TRACEDIR=%ROOT%\recomp\fase4\traces"
set "STATUS=%ROOT%\tracy_export.status"
set "LOG=%ROOT%\tracy_export.log"

echo RUNNING> "%STATUS%"
echo Tracy CSV export started %DATE% %TIME%> "%LOG%"

if not exist "%EXPORT%" (
    echo Tracy csvexport tool not found: %EXPORT%>> "%LOG%"
    echo FAIL> "%STATUS%"
    echo Tracy csvexport tool not found:
    echo %EXPORT%
    pause
    exit /b 1
)

set "FAILED=0"

call :export narutobb_cutin_jutsu
call :export narutobb_cutin_jutsu_30fps
call :export narutobb_battle_control
call :export narutobb_battle_profile

if "%FAILED%"=="1" (
    echo FAIL> "%STATUS%"
    echo.
    echo One or more exports failed. See tracy_export.log
    pause
    exit /b 1
)

echo OK> "%STATUS%"
echo.
echo CSV files written to:
echo %TRACEDIR%
pause
exit /b 0

:export
set "NAME=%~1"
set "SRC=%TRACEDIR%\%NAME%.tracy"
set "DST=%TRACEDIR%\%NAME%.csv"

if not exist "%SRC%" (
    echo SKIP %NAME%: capture file not found>> "%LOG%"
    echo Skipping %NAME% - capture file not found.
    goto :eof
)

echo Exporting %NAME% ...
echo ---- %NAME% ---->> "%LOG%"
"%EXPORT%" "%SRC%" > "%DST%" 2>> "%LOG%"
if errorlevel 1 (
    echo FAILED %NAME%>> "%LOG%"
    set "FAILED=1"
    goto :eof
)
for %%F in ("%DST%") do echo OK %NAME% bytes=%%~zF>> "%LOG%"
goto :eof
