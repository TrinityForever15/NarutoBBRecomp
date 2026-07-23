@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "BUILDDIR=%ROOT%\native\narutobb\out\build\win-amd64-source"
set "LOG=%ROOT%\recomp\fase4\build_bisect.log"
set "STATUS=%ROOT%\recomp\fase4\build_bisect.status"

echo RUNNING codegen> "%STATUS%"
echo Regenerating guest code from the manifest...
cmake --build "%BUILDDIR%" --target narutobb_codegen > "%LOG%" 2>&1
if errorlevel 1 goto :fail

rem Codegen may change the generated source list; reconfigure before building.
echo RUNNING configure> "%STATUS%"
cmake -S "%ROOT%\native\narutobb" -B "%BUILDDIR%" >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo RUNNING game+runtime> "%STATUS%"
echo Building game and runtime...
cmake --build "%BUILDDIR%" --config Release --parallel 4 >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo RUNNING trace_dump> "%STATUS%"
echo Building the replay tool...
cmake --build "%BUILDDIR%" --config Release --target narutobb_trace_dump --parallel 4 >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo OK> "%STATUS%"
echo Build complete. Log: %LOG%
exit /b 0

:fail
echo FAIL> "%STATUS%"
echo Build failed. See: %LOG%
exit /b 1
