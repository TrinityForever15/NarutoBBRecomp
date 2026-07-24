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

echo OPEN-WORLD 60 FPS TIMING EXPERIMENT
echo Enter the open world normally. The experiment starts disarmed.
echo After gameplay is stable, press F8 to arm 120 Hz guest vblank and 1/60 simulation.
echo F3 shows guest FPS. Further F8 presses restore or rearm 60 Hz.
echo After the world guard validates, the mode remains active through pause and transitions.
echo Press F8 before returning to the front-end if you want the original timing there.
echo The expensive timing trace starts disabled; press F10 only for a short capture if requested.
echo Observe movement, animation, camera, physics, audio, and game-time speed.

start "" /D "%BUILDDIR%" "%GAME%" "%ASSETS%" --game_data_root "%ASSETS%" --log-level info --mnk_mode --keybind_start Return --audio_maxqframes=64 --render_target_path_d3d12=rtv --trace_gpu_prefix "%TRACEDIR%" --naruto_world_120hz_vblank_experiment=true
exit /b 0
