@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
cd /d "%ROOT%"
set "STATUS=%ROOT%\recomp\fase4\bisect_run.status"
set "LOG=%ROOT%\recomp\fase4\bisect_run.log"

echo RUNNING> "%STATUS%"
echo Bisecting 55530825_5582 (fully black)...
powershell -NoProfile -ExecutionPolicy Bypass -File recomp\fase4\bisect_black_draw.ps1 -Trace recomp\fase4\traces\55530825_5582.xtr -SweepCopies > "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo Bisecting 55530825_5935 (nearly black)...
powershell -NoProfile -ExecutionPolicy Bypass -File recomp\fase4\bisect_black_draw.ps1 -Trace recomp\fase4\traces\55530825_5935.xtr -SweepCopies >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo Bisecting 55530825_6621 (purple composition)...
powershell -NoProfile -ExecutionPolicy Bypass -File recomp\fase4\bisect_black_draw.ps1 -Trace recomp\fase4\traces\55530825_6621.xtr -SweepCopies >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo OK> "%STATUS%"
exit /b 0

:fail
echo FAIL> "%STATUS%"
exit /b 1
