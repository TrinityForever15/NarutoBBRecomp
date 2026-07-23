param(
    [string]$BuildDir = "native/narutobb/out/build/verification-clean",
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Configuration = "Release",
    [ValidateRange(1, 32)]
    [int]$Parallel = 4,
    [switch]$SkipCodegen
)

$ErrorActionPreference = "Stop"
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$ProjectDir = Join-Path $ProjectRoot "native/narutobb"
$SdkDir = Join-Path $ProjectRoot "tooling/rexglue-sdk"
$AssetsDir = Join-Path $ProjectRoot "recomp/fase4/assets"
$Xex = Join-Path $AssetsDir "default.xex"
$ResolvedBuildDir = if ([IO.Path]::IsPathRooted($BuildDir)) {
    [IO.Path]::GetFullPath($BuildDir)
} else {
    [IO.Path]::GetFullPath((Join-Path $ProjectRoot $BuildDir))
}

function Invoke-Checked {
    param([string]$Label, [scriptblock]$Command)
    Write-Host "[$Label]"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

function Find-Tool {
    param([string]$Name, [string[]]$Candidates)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source.Replace('\', '/') }
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path.Replace('\', '/')
        }
    }
    throw "Tool not found: $Name"
}

foreach ($required in @($ProjectDir, $SdkDir, $Xex)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required dependency is missing: $required"
    }
}

$cmake = Find-Tool "cmake" @("C:/Program Files/CMake/bin/cmake.exe")
$clang = Find-Tool "clang" @("C:/Program Files/LLVM/bin/clang.exe")
$clangxx = Find-Tool "clang++" @("C:/Program Files/LLVM/bin/clang++.exe")
$rcCandidates = @(
    @(Get-ChildItem -Path "C:/Program Files (x86)/Windows Kits/10/bin" -Recurse -Filter "rc.exe" -File -ErrorAction SilentlyContinue |
        Where-Object FullName -Match '[/\\]x64[/\\]rc\.exe$' |
        Sort-Object FullName -Descending | Select-Object -ExpandProperty FullName),
    "C:/Program Files/LLVM/bin/llvm-rc.exe"
)
$rc = Find-Tool "rc" $rcCandidates
$ninjaCandidates = @(
    "C:/Program Files/Ninja/ninja.exe",
    @(Get-ChildItem -Path "$env:LOCALAPPDATA/Microsoft/WinGet/Packages" -Recurse -Filter "ninja.exe" -File -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName)
)
$ninja = Find-Tool "ninja" $ninjaCandidates

# Paths written into CMake files must use forward slashes. A Windows path such
# as "C:\Program Files" otherwise contains "\P", which CMake parses as an
# invalid escape sequence.
$ProjectDirCMake = $ProjectDir.Replace('\', '/')
$SdkDirCMake = $SdkDir.Replace('\', '/')
$ResolvedBuildDirCMake = $ResolvedBuildDir.Replace('\', '/')

New-Item -ItemType Directory -Force -Path $ResolvedBuildDir | Out-Null

Invoke-Checked "initial configure" {
    & $cmake -S $ProjectDirCMake -B $ResolvedBuildDirCMake -G Ninja `
        "-DCMAKE_BUILD_TYPE=$Configuration" `
        "-DCMAKE_C_COMPILER=$clang" `
        "-DCMAKE_CXX_COMPILER=$clangxx" `
        "-DCMAKE_C_FLAGS=-mssse3" `
        "-DCMAKE_CXX_FLAGS=-mssse3" `
        "-DCMAKE_RC_COMPILER=$rc" `
        "-DCMAKE_MAKE_PROGRAM=$ninja" `
        "-DREXSDK_DIR=$SdkDirCMake" `
        "-DREXGLUE_ENABLE_TRACY=OFF"
}

if (-not $SkipCodegen) {
    Invoke-Checked "codegen" {
        & $cmake --build $ResolvedBuildDirCMake --target narutobb_codegen --parallel $Parallel
    }
    # Codegen may change sources.cmake; reconfigure to load the new source list.
    Invoke-Checked "reconfigure after codegen" {
        & $cmake -S $ProjectDirCMake -B $ResolvedBuildDirCMake
    }
}

Invoke-Checked "build game and runtime" {
    & $cmake --build $ResolvedBuildDirCMake --target narutobb --parallel $Parallel
}
Invoke-Checked "build trace dump" {
    & $cmake --build $ResolvedBuildDirCMake --target narutobb_trace_dump --parallel $Parallel
}

$gitCandidates = @(
    "C:/Program Files/Git/cmd/git.exe",
    "C:/Program Files/Git/bin/git.exe"
)
$git = try { Find-Tool "git" $gitCandidates } catch { $null }
$sdkHead = if ($git) {
    (& $git -C $SdkDir rev-parse HEAD 2>$null)
} else { "unknown" }
$exe = Join-Path $ResolvedBuildDir "narutobb.exe"
$dll = Join-Path $ResolvedBuildDir "rexruntime.dll"
$traceDump = Join-Path $ResolvedBuildDir "narutobb_trace_dump.exe"
foreach ($output in @($exe, $dll, $traceDump)) {
    if (-not (Test-Path -LiteralPath $output)) {
        throw "Build completed without producing: $output"
    }
}

$provenance = [ordered]@{
    generated_at = (Get-Date).ToString("o")
    project_root = $ProjectRoot
    build_dir = $ResolvedBuildDir
    configuration = $Configuration
    sdk_head = "$sdkHead".Trim()
    xex_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $Xex).Hash
    outputs = [ordered]@{
        narutobb_exe = (Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash
        rexruntime_dll = (Get-FileHash -Algorithm SHA256 -LiteralPath $dll).Hash
        trace_dump_exe = (Get-FileHash -Algorithm SHA256 -LiteralPath $traceDump).Hash
    }
}
$provenance | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $ResolvedBuildDir "build_provenance.json") -Encoding UTF8

Write-Host "BUILD_CLEAN_OK $ResolvedBuildDir"
Write-Host "narutobb.exe $($provenance.outputs.narutobb_exe)"
Write-Host "rexruntime.dll $($provenance.outputs.rexruntime_dll)"
Write-Host "narutobb_trace_dump.exe $($provenance.outputs.trace_dump_exe)"
