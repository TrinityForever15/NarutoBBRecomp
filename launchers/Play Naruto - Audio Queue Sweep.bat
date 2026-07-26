@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "BUILDDIR=%ROOT%\native\narutobb\out\build\win-amd64-source"
set "GAME=%BUILDDIR%\narutobb.exe"
set "ASSETS=%ROOT%\recomp\fase4\assets"
set "TRACEDIR=%ROOT%\recomp\fase4\traces"

if not exist "%GAME%" (
    echo Native executable not found:
    echo %GAME%
    pause
    exit /b 1
)

if not exist "%ASSETS%\default.xex" (
    echo Local game data not found:
    echo %ASSETS%
    pause
    exit /b 1
)

if not exist "%TRACEDIR%" mkdir "%TRACEDIR%"

echo.
echo NARUTO - AUDIO OUTPUT QUEUE SWEEP
echo.
echo Tests whether output backpressure is delaying the guest thread that feeds
echo XMA input buffers. The guest only receives a frame credit when the audio
echo callback releases the semaphore, so a small queue could throttle it.
echo.
echo Run the SAME cutscene at each depth, bracketed with F7.
echo.
echo   1  16 frames   about  85 ms buffered
echo   2  32 frames   about 171 ms buffered
echo   3  64 frames   about 341 ms buffered   (current default)
echo   4  128 frames  about 683 ms buffered
echo.
echo The log records NARUTO_AUDIO_QUEUE_CONFIG at startup, so each capture
echo identifies its own setting.
echo.

choice /c 1234 /n /m "Queue depth [1-4]: "
if errorlevel 4 set "QFRAMES=128"
if errorlevel 4 goto :run
if errorlevel 3 set "QFRAMES=64"
if errorlevel 3 goto :run
if errorlevel 2 set "QFRAMES=32"
if errorlevel 2 goto :run
set "QFRAMES=16"

:run
echo.
echo Starting with audio_maxqframes=%QFRAMES%
echo.
echo What decides the question, in the log:
echo   If stall_slow and stall_timeouts track the queue depth, output
echo   backpressure is a real factor and a different audio backend could help.
echo   If they stay flat across all four, the output path is ruled out for
echo   good and the remaining work is entirely in the XMA layer.
echo.

start "" /D "%BUILDDIR%" "%GAME%" "%ASSETS%" --game_data_root "%ASSETS%" --log-level info --mnk_mode --keybind_start Return --audio_maxqframes=%QFRAMES% --render_target_path_d3d12=rtv --trace_gpu_prefix "%TRACEDIR%" --naruto_xma_stall_on_missing_input=true --naruto_xma_stream_telemetry=true --naruto_audio_flow_telemetry=true --naruto_60fps_experiment=true --naruto_60fps_on_start=true --naruto_target_frame_rate=60 --naruto_pacing_target_hz=60 --naruto_pacing_lead_ms=0
exit /b 0
