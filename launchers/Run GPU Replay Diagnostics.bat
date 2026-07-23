@echo off
setlocal enabledelayedexpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
cd /d "%ROOT%"
set "STATUS=%ROOT%\recomp\fase4\diag_extra.status"
set "LOG=%ROOT%\recomp\fase4\diag_extra.log"
set "BUILDDIR=%ROOT%\native\narutobb\out\build\win-amd64-source"
set "OUT=%ROOT%\recomp\fase4\traces\replay_fix"
if not exist "%OUT%" mkdir "%OUT%"
echo RUNNING build> "%STATUS%"

cmake --build "%BUILDDIR%" --target narutobb_codegen > "%LOG%" 2>&1
if errorlevel 1 goto :fail
cmake -S "%ROOT%\native\narutobb" -B "%BUILDDIR%" >> "%LOG%" 2>&1
if errorlevel 1 goto :fail
cmake --build "%BUILDDIR%" --config Release --parallel 4 >> "%LOG%" 2>&1
if errorlevel 1 goto :fail
cmake --build "%BUILDDIR%" --config Release --target narutobb_trace_dump --parallel 4 >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo RUNNING replays> "%STATUS%"
for %%T in (3550 3819 4756 4804 5582 5935 6013 6136 6441 6621 7168 7617) do (
    echo === replay %%T === >> "%LOG%"
    "%BUILDDIR%\narutobb_trace_dump.exe" "%ROOT%\recomp\fase4\traces\55530825_%%T.xtr" "%OUT%\55530825_%%T" --render_target_path_d3d12=rtv --depth_transfer_not_equal_test=true --async_shader_compilation=false >> "%LOG%" 2>&1
    if errorlevel 1 echo FAIL_%%T>> "%STATUS%"
)

echo OK> "%STATUS%"
exit /b 0

:fail
echo FAIL> "%STATUS%"
exit /b 1
