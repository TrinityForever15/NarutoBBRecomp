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

echo MENU 60 FPS TIMING EXPERIMENT
echo Stay in the title and front-end menus. Do not enter gameplay in this run.
echo Wait until the front-end menu is visible, then press F8 to arm 120 Hz and 1/60 simulation.
echo F3 shows guest FPS. Further F8 presses suspend or rearm the intervention.
echo The expensive timing trace starts disabled; press F10 only if requested.
echo Observe whether menu animation, fades, and scrolling retain normal speed.

start "" /D "%BUILDDIR%" "%GAME%" "%ASSETS%" --game_data_root "%ASSETS%" --log-level info --mnk_mode --keybind_start Return --audio_maxqframes=64 --render_target_path_d3d12=rtv --trace_gpu_prefix "%TRACEDIR%" --naruto_menu_120hz_vblank_experiment=true
exit /b 0
