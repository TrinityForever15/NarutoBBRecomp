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
echo NARUTO - CUTSCENE AUDIO TELEMETRY
echo.
echo Phase 1 of the audio plan. Nothing about audio behaviour changes; this
echo build only counts and reports.
echo.
echo Two reports appear in the log once per second:
echo   NARUTO_XMA_STREAM  one line per active stream, with fill_ratio
echo   NARUTO_AUDIO_FLOW  output queue depth, underrun time, discontinuities
echo.
echo Press F7 right before a cutscene starts and again when it ends. That
echo writes NARUTO_AUDIO_MARK and brackets the scene exactly.
echo.
echo Capture in this order:
echo   1. The cutscene that always fails, bracketed with F7.
echo   2. One cutscene per remaining symptom, each bracketed.
echo   3. One cutscene whose audio is correct, as the control.
echo   4. The failing one again with F8 pressed, so the frame rate mode is
echo      suspended and a timing interaction can be ruled out.
echo.
echo Say out loud or note which symptom you heard between each F7 pair.
echo.

start "" /D "%BUILDDIR%" "%GAME%" "%ASSETS%" --game_data_root "%ASSETS%" --log-level info --mnk_mode --keybind_start Return --audio_maxqframes=64 --render_target_path_d3d12=rtv --trace_gpu_prefix "%TRACEDIR%" --naruto_xma_stream_telemetry=true --naruto_audio_flow_telemetry=true --audio_silence_diagnostics=true --naruto_60fps_experiment=true --naruto_60fps_on_start=true --naruto_target_frame_rate=60 --naruto_pacing_target_hz=60 --naruto_pacing_lead_ms=0
exit /b 0
