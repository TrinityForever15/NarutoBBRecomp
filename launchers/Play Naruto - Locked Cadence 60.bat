@echo off
setlocal

rem Target cadence. The simulation step and the presentation pacer are both
rem derived from this single number, so they can never disagree. Change both
rem values together to try a higher rate, for example 90 or 120.
set "TARGET_HZ=60"

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
echo NARUTO - LOCKED CADENCE AT %TARGET_HZ% HZ
echo.
echo Telemetry proved battle runs on a fixed timestep, where simulation speed
echo is exactly achieved_fps multiplied by the step. This build derives the
echo step and the pacer target from the same number so they cannot drift
echo apart.
echo.
echo The experiment still starts disarmed. Press F8 once to enable it.
echo.
echo What to check:
echo   1. Menu and open world unchanged.
echo   2. In battle, NARUTO_SIM_CADENCE raw_speed must sit at 1.000.
echo   3. peak_speed shows the worst instantaneous frame, not the average.
echo   4. Use the STATUS jutsu with the portrait and watch peak_speed.
echo.
echo The original XEX is never modified.
echo.

rem Measured on this machine: the pacer lands on its internal target almost
rem exactly, so the historical 0.406 ms lead only made it overshoot to 61.5 Hz
rem against a 60 Hz goal. Zero lead makes the achieved cadence match the step.
start "" /D "%BUILDDIR%" "%GAME%" "%ASSETS%" --game_data_root "%ASSETS%" --log-level info --mnk_mode --keybind_start Return --audio_maxqframes=64 --render_target_path_d3d12=rtv --trace_gpu_prefix "%TRACEDIR%" --naruto_60fps_experiment=true --naruto_target_frame_rate=%TARGET_HZ% --naruto_pacing_target_hz=%TARGET_HZ% --naruto_pacing_lead_ms=0
exit /b 0
