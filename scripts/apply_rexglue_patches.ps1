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
$PatchDir = Join-Path $ProjectRoot "patches/rexglue-sdk"
$Base = "2bdb97f95f154f32d281aaa08446ae007b8ca117"
$ExpectedHead = "c91f2b53a1018b779ed3b5d9d201412719cb73ca"
$ExpectedTree = "62e97f17f8e6cfb4d73905c5158aef1d8d292151"
$gitCommand = Get-Command git -ErrorAction SilentlyContinue
$git = if ($gitCommand) { $gitCommand.Source } else {
    $candidate = @("C:/Program Files/Git/cmd/git.exe", "C:/Program Files/Git/bin/git.exe") |
        Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $candidate) { throw "Git was not found" }
    $candidate
}

if (-not (Test-Path -LiteralPath (Join-Path $ResolvedSdkDir ".git"))) {
    throw "SDK path is not a Git clone: $ResolvedSdkDir"
}
$head = (& $git -C $ResolvedSdkDir rev-parse HEAD).Trim()
$tree = (& $git -C $ResolvedSdkDir rev-parse 'HEAD^{tree}').Trim()
if ($head -eq $ExpectedHead -or $tree -eq $ExpectedTree) {
    Write-Host "REXGLUE_PATCHES_ALREADY_APPLIED head=$head tree=$tree"
    exit 0
}
if ($head -ne $Base) {
    throw "SDK is at unexpected commit: $head (expected base $Base or head $ExpectedHead)"
}
$dirty = @(& $git -C $ResolvedSdkDir status --porcelain --ignore-submodules=dirty)
if ($dirty.Count) {
    throw "SDK has local changes; patches were not applied so existing work is preserved"
}
$patches = @(Get-ChildItem -LiteralPath $PatchDir -Filter "*.patch" -File | Sort-Object Name)
if ($patches.Count -ne 12) { throw "Expected 12 patches, found $($patches.Count)" }

foreach ($patch in $patches) {
    Write-Host "[git am] $($patch.Name)"
    & $git -C $ResolvedSdkDir am $patch.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to apply $($patch.Name). Resolve it or run git am --abort in the SDK checkout."
    }
}
$newHead = (& $git -C $ResolvedSdkDir rev-parse HEAD).Trim()
$newTree = (& $git -C $ResolvedSdkDir rev-parse 'HEAD^{tree}').Trim()
if ($newTree -ne $ExpectedTree) {
    throw "Patch series produced an unexpected tree: $newTree"
}
Write-Host "REXGLUE_PATCHES_OK head=$newHead tree=$newTree"
