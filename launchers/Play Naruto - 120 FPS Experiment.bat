@echo off
setlocal

rem Target cadence. The simulation step, the presentation pacer and the guest
rem vblank multiplier are all derived from this one number, so they cannot
rem disagree. The runtime clamps the multiplier to four, which caps the
rem reachable target at 120.
set "TARGET_HZ=120"

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
echo NARUTO AT %TARGET_HZ% FPS - EXPERIMENTAL
echo.
echo The frame rate mode arms itself shortly after boot. No key press is
echo needed. F8 still suspends it and restores original timing at any time.
echo.
echo The original XEX is never modified.
echo.
echo WARNING - this target is not validated.
echo   - Fighting games often encode move timing in frames rather than time.
echo     Combo windows, invulnerability and input buffering can change even
echo     when NARUTO_SIM_CADENCE reports raw_speed 1.000.
echo   - Battle physics is tuned for a fixed step; 120 may alter feel or
echo     scripted sequences.
echo   - Battle frames were measured at 19-23 ms in places, and 120 FPS needs
echo     8.3 ms, so expect the pacer to give up under load.
echo.
echo Judge this against combat timing, not against how smooth it looks.
echo.
echo To check whether the speed telemetry itself costs frames, relaunch with
echo --naruto_sim_cadence_telemetry=false appended to the command below and
echo compare. The log loses NARUTO_SIM_CADENCE while it is off.
echo.

start "" /D "%BUILDDIR%" "%GAME%" "%ASSETS%" --game_data_root "%ASSETS%" --log-level info --mnk_mode --keybind_start Return --audio_maxqframes=64 --render_target_path_d3d12=rtv --trace_gpu_prefix "%TRACEDIR%" --naruto_60fps_experiment=true --naruto_60fps_on_start=true --naruto_target_frame_rate=%TARGET_HZ% --naruto_pacing_target_hz=%TARGET_HZ% --naruto_pacing_lead_ms=0
exit /b 0
