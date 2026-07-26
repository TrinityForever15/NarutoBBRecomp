@echo off
setlocal

rem Fast rebuild path for changes to game hooks or runtime source. It does not
rem run code generation, so use 'Rebuild Diagnostics.bat' instead whenever a
rem guest-function manifest changed.

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "BUILDDIR=%ROOT%\native\narutobb\out\build\win-amd64-source"
set "STATUS=%ROOT%\build.status"
set "LOG=%ROOT%\build.log"

echo RUNNING> "%STATUS%"
echo Build started %DATE% %TIME%> "%LOG%"

if not exist "%BUILDDIR%\build.ninja" (
    echo Configured build directory not found: %BUILDDIR%>> "%LOG%"
    echo FAIL> "%STATUS%"
    echo Configured build directory not found:
    echo %BUILDDIR%
    pause
    exit /b 1
)

rem Build everything rather than only the game target, because runtime sources
rem under tooling/rexglue-sdk are part of this configuration.
echo Building runtime and game ...
cmake --build "%BUILDDIR%" >> "%LOG%" 2>&1
if errorlevel 1 (
    echo FAIL> "%STATUS%"
    echo.
    echo Build FAILED. Last lines:
    powershell -NoProfile -Command "Get-Content -Tail 30 '%LOG%'"
    pause
    exit /b 1
)

echo OK> "%STATUS%"
echo.
echo Build OK. Log: %LOG%
pause
exit /b 0
