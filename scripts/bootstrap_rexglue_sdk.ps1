param(
    [string]$SdkDir = "tooling/rexglue-sdk"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$ResolvedSdkDir = if ([IO.Path]::IsPathRooted($SdkDir)) {
    [IO.Path]::GetFullPath($SdkDir)
} else {
    [IO.Path]::GetFullPath((Join-Path $ProjectRoot $SdkDir))
}
$Base = "2bdb97f95f154f32d281aaa08446ae007b8ca117"
$ExpectedTree = "62e97f17f8e6cfb4d73905c5158aef1d8d292151"
$Repository = "https://github.com/rexglue/rexglue-sdk.git"
$gitCandidates = @("C:/Program Files/Git/cmd/git.exe", "C:/Program Files/Git/bin/git.exe")
$gitCommand = Get-Command git -ErrorAction SilentlyContinue
$git = if ($gitCommand) { $gitCommand.Source } else {
    $candidate = $gitCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $candidate) { throw "Git was not found" }
    $candidate
}

if (-not (Test-Path -LiteralPath (Join-Path $ResolvedSdkDir ".git"))) {
    if (Test-Path -LiteralPath $ResolvedSdkDir) {
        throw "SDK destination exists but is not a Git clone: $ResolvedSdkDir"
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ResolvedSdkDir) | Out-Null
    & $git clone --recurse-submodules $Repository $ResolvedSdkDir
    if ($LASTEXITCODE -ne 0) { throw "ReXGlue clone failed" }
}

$tree = (& $git -C $ResolvedSdkDir rev-parse 'HEAD^{tree}' 2>$null).Trim()
$head = (& $git -C $ResolvedSdkDir rev-parse HEAD 2>$null).Trim()
if ($tree -ne $ExpectedTree -and $head -ne $Base) {
    $dirty = @(& $git -C $ResolvedSdkDir status --porcelain --ignore-submodules=dirty)
    if ($dirty.Count) {
        throw "SDK has local changes and an unexpected tree; refusing to replace the checkout"
    }
    & $git -C $ResolvedSdkDir fetch origin $Base --tags
    if ($LASTEXITCODE -ne 0) { throw "Could not fetch pinned upstream base $Base" }
    & $git -C $ResolvedSdkDir checkout -B narutobb-integration $Base
    if ($LASTEXITCODE -ne 0) { throw "Could not check out pinned upstream base $Base" }
    & $git -C $ResolvedSdkDir submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw "Failed to initialize SDK submodules" }
}

& (Join-Path $PSScriptRoot "apply_rexglue_patches.ps1") -SdkDir $ResolvedSdkDir
Write-Host "REXGLUE_BOOTSTRAP_OK $ResolvedSdkDir"
