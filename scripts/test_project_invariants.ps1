param()

$ErrorActionPreference = "Stop"
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Failures = [Collections.Generic.List[string]]::new()

function Get-ProjectRelativePath([string]$Path) {
    $RootWithSlash = $ProjectRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $RootUri = [Uri]::new($RootWithSlash)
    $PathUri = [Uri]::new([IO.Path]::GetFullPath($Path))
    return [Uri]::UnescapeDataString($RootUri.MakeRelativeUri($PathUri).ToString()).Replace('/', [IO.Path]::DirectorySeparatorChar)
}

$RequiredDocs = @(
    "README.md",
    "AGENTS.md",
    "SKILL.md",
    "CONTRIBUTING.md",
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "docs/STATUS.md",
    "docs/PROJECT_STRUCTURE.md",
    "docs/TECHNICAL_DECISIONS.md",
    "docs/TEST_MATRIX.md",
    "docs/LEGAL.md"
)

foreach ($Relative in $RequiredDocs) {
    $Path = Join-Path $ProjectRoot $Relative
    if (-not (Test-Path -LiteralPath $Path)) {
        $Failures.Add("Missing required document: $Relative")
    }
}

# Validate local Markdown links in all public documentation.
$MarkdownFiles = @(
    Get-ChildItem -LiteralPath $ProjectRoot -File -Filter "*.md"
    Get-ChildItem -LiteralPath (Join-Path $ProjectRoot "docs") -File -Recurse -Filter "*.md"
    Get-ChildItem -LiteralPath (Join-Path $ProjectRoot "recomp") -File -Recurse -Filter "*.md"
    Get-ChildItem -LiteralPath (Join-Path $ProjectRoot "scripts") -File -Filter "*.md"
    Get-ChildItem -LiteralPath (Join-Path $ProjectRoot "patches") -File -Recurse -Filter "*.md"
) | Sort-Object FullName -Unique

foreach ($File in $MarkdownFiles) {
    $Content = Get-Content -LiteralPath $File.FullName -Raw -Encoding UTF8
    foreach ($Match in [regex]::Matches($Content, '\[[^\]]+\]\(([^)]+)\)')) {
        $Target = $Match.Groups[1].Value.Trim('<', '>')
        if ($Target -match '^(https?://|mailto:|#)') { continue }
        $Target = [Uri]::UnescapeDataString($Target.Split('#')[0])
        if (-not $Target) { continue }
        $Full = [IO.Path]::GetFullPath((Join-Path $File.DirectoryName $Target))
        if (-not (Test-Path -LiteralPath $Full)) {
            $RelativeFile = Get-ProjectRelativePath $File.FullName
            $Failures.Add("Broken Markdown link in $RelativeFile`: $Target")
        }
    }
}

# Both manifests must contain exactly the same explicit guest-function ranges.
$ManifestA = Join-Path $ProjectRoot "native/narutobb/narutobb_manifest.toml"
$ManifestB = Join-Path $ProjectRoot "recomp/fase4/narutobb_manifest.toml"
function Get-FunctionRanges([string]$Path) {
    return @(Get-Content -LiteralPath $Path -Encoding UTF8 |
        Where-Object { $_ -match '^0x[0-9A-Fa-f]+\s*=\s*\{' } |
        ForEach-Object { $_.Trim() } |
        Sort-Object)
}
$RangesA = Get-FunctionRanges $ManifestA
$RangesB = Get-FunctionRanges $ManifestB
if (Compare-Object $RangesA $RangesB) {
    $Failures.Add("The two manifests contain different guest-function ranges")
}

# Local game data is optional for a public checkout. If present, it must be the
# known original input and not a patched experiment.
$Xex = Join-Path $ProjectRoot "recomp/fase4/assets/default.xex"
if (Test-Path -LiteralPath $Xex) {
    $ExpectedXex = "8F70E79443E36B38E44B6A105DAD51FB8909FB615D982753CABAAC9B15F0D576"
    $ActualXex = (Get-FileHash -Algorithm SHA256 -LiteralPath $Xex).Hash
    if ($ActualXex -ne $ExpectedXex) {
        $Failures.Add("The active default.xex is not the known original input: $ActualXex")
    }
}

$Register = Join-Path $ProjectRoot "native/narutobb/generated/default/narutobb_register.cpp"
if ((Test-Path -LiteralPath $Register) -and
    -not (Select-String -LiteralPath $Register -Quiet -Pattern 'SetFunction\(0x8215D000')) {
    $Failures.Add("Guest function 0x8215D000 is missing from generated registration")
}

# Audit the exact files that would be included by Git, including new untracked
# files that are not ignored yet.
$CandidateFiles = @(
    git -C $ProjectRoot ls-files --cached --others --exclude-standard
) | Sort-Object -Unique | Where-Object { Test-Path -LiteralPath (Join-Path $ProjectRoot $_) -PathType Leaf }

$ForbiddenPathPatterns = @(
    '^local-research/',
    '^recomp/fase4/assets/',
    '^recomp/fase4/evidencias/',
    '^recomp/fase4/traces/',
    '^native/narutobb/generated/default/',
    '^native/narutobb/out/',
    '^tooling/rexglue-sdk/'
)
$ForbiddenExtensions = @(
    '.iso', '.xex', '.bik', '.sra', '.bf', '.sdb', '.xzp', '.xvu', '.xvd',
    '.stfs', '.sav', '.exe', '.dll', '.pdb', '.bmp', '.xtr', '.dmp'
)

foreach ($Relative in $CandidateFiles) {
    $Normalized = $Relative.Replace('\', '/')
    foreach ($Pattern in $ForbiddenPathPatterns) {
        if ($Normalized -match $Pattern) {
            $Failures.Add("Forbidden public path: $Relative")
            break
        }
    }

    $Extension = [IO.Path]::GetExtension($Relative).ToLowerInvariant()
    if ($ForbiddenExtensions -contains $Extension) {
        $Failures.Add("Forbidden public file type: $Relative")
    }

    $Full = Join-Path $ProjectRoot $Relative
    if ((Test-Path -LiteralPath $Full -PathType Leaf) -and
        (Get-Item -LiteralPath $Full).Length -gt 5MB) {
        $Failures.Add("Unexpected public file larger than 5 MiB: $Relative")
    }
}

# Block known high-risk material and common credential forms in public text.
$TextExtensions = @('.md', '.txt', '.py', '.ps1', '.bat', '.sh', '.cpp', '.h', '.toml', '.json', '.cmake')
$SensitivePatterns = @(
    ('20B185A59D28FDC3' + '40583FBB0896BF91'),
    '-----BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY-----',
    '(?i)(api[_-]?key|client[_-]?secret|access[_-]?token|password)\s*[:=]\s*["''][^"'']{8,}'
)
foreach ($Relative in $CandidateFiles) {
    $Full = Join-Path $ProjectRoot $Relative
    if (-not (Test-Path -LiteralPath $Full -PathType Leaf)) { continue }
    $Extension = [IO.Path]::GetExtension($Relative).ToLowerInvariant()
    if (($TextExtensions -notcontains $Extension) -and
        ([IO.Path]::GetFileName($Relative) -notin @('LICENSE', '.gitignore', '.gitattributes'))) {
        continue
    }
    $Content = Get-Content -LiteralPath $Full -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
    foreach ($Pattern in $SensitivePatterns) {
        if ($Content -match $Pattern) {
            $Failures.Add("Sensitive material pattern found in: $Relative")
            break
        }
    }
}

if ($Failures.Count) {
    $Failures | Sort-Object -Unique | ForEach-Object { Write-Error $_ }
    throw "PROJECT_INVARIANTS_FAIL count=$($Failures.Count)"
}

Write-Host "PROJECT_INVARIANTS_OK docs=$($RequiredDocs.Count) functions=$($RangesA.Count) public_files=$($CandidateFiles.Count)"
