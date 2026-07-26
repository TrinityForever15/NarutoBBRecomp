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
echo NARUTO - XMA INPUT STALL FIX TEST
echo.
echo Measurement found that every decode error in the previous session was the
echo same case: a frame split across two input buffers whose second buffer the
echo guest had not supplied yet. That is a wait, not a stream error.
echo.
echo This build treats that case as a stall. The read offset is left in place
echo and no error is reported to the guest, so the same fragment is no longer
echo replayed and the stream is not told it failed.
echo.
echo Play the SAME cutscene as before, bracketed with F7.
echo.
echo In the log, expect decode_errors to fall to near zero and stalls to rise
echo by roughly the same amount. What matters more is what you hear: whether
echo the repeating scream, the missing audio and the hiss are gone.
echo.
echo The wait is bounded by naruto_xma_stall_timeout_ms. An unbounded wait was
echo tested first and left seven streams silent forever, one for 21 seconds,
echo because a stream that ends on a split frame never gets another buffer.
echo.
echo In the log, stall_timeouts should be small and non-zero: those are streams
echo ending normally. If a stream still goes quiet and never returns, the
echo timeout is too long. To try a shorter one, append
echo --naruto_xma_stall_timeout_ms=40 to the command below.
echo.

start "" /D "%BUILDDIR%" "%GAME%" "%ASSETS%" --game_data_root "%ASSETS%" --log-level info --mnk_mode --keybind_start Return --audio_maxqframes=64 --render_target_path_d3d12=rtv --trace_gpu_prefix "%TRACEDIR%" --naruto_xma_stall_on_missing_input=true --naruto_xma_stream_telemetry=true --naruto_audio_flow_telemetry=true --audio_silence_diagnostics=true --naruto_60fps_experiment=true --naruto_60fps_on_start=true --naruto_target_frame_rate=60 --naruto_pacing_target_hz=60 --naruto_pacing_lead_ms=0
exit /b 0
