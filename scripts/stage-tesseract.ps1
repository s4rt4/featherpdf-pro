# Feather PDF Pro — stage a bundled Tesseract for the installer.
#
# Tesseract has no official Windows binaries, so the bundled copy comes from
# vcpkg (tesseract.exe plus its DLLs land in installed/x64-windows/tools) and
# the language data from the tessdata_fast models (small, LSTM-only — right
# for OCR that runs interactively). Everything is staged under
# <staging>/bin/tools/tesseract, which is where ToolLocator::tesseract()
# looks first; the app passes the tessdata directory to the exe through
# TESSDATA_PREFIX because a vcpkg build's compiled-in data path points at the
# build machine.
#
# Run after `cmake --install build --prefix staging`, before iscc:
#   .\scripts\stage-tesseract.ps1 [-Staging staging] [-Langs eng,ind]
#
# The Inno Setup script picks the staged copy up as the optional "ocr"
# component and skips it when this script was never run.

param(
    [string]$Staging = "staging",
    [string]$VcpkgRoot = "C:/vcpkg",
    [string[]]$Langs = @("eng", "ind"),
    # tessdata_fast commit to fetch models from; a tag/branch also works.
    [string]$TessdataRef = "main"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
if (-not [IO.Path]::IsPathRooted($Staging)) { $Staging = Join-Path $repo $Staging }

$tools = Join-Path $VcpkgRoot "installed/x64-windows/tools/tesseract"
if (-not (Test-Path "$tools/tesseract.exe")) {
    throw "vcpkg's tesseract tool not found at $tools - run: vcpkg install tesseract:x64-windows"
}

$dest = Join-Path $Staging "bin/tools/tesseract"
New-Item -ItemType Directory -Force $dest | Out-Null
# vcpkg deploys the dependent DLLs (leptonica, libpng, ...) beside the exe.
Copy-Item "$tools/*" $dest -Recurse -Force -Exclude *.pdb

# Language models. osd comes along whenever more than one language is bundled
# so the app's auto-detect (Tesseract OSD, --psm 0) works out of the box.
$tessdata = Join-Path $dest "tessdata"
New-Item -ItemType Directory -Force $tessdata | Out-Null
$models = [System.Collections.Generic.List[string]]::new()
$Langs | ForEach-Object { $models.Add($_) }
if ($models.Count -gt 1 -and -not $models.Contains("osd")) { $models.Add("osd") }
foreach ($model in $models) {
    $out = Join-Path $tessdata "$model.traineddata"
    if (Test-Path $out) { Write-Host "already staged: $model.traineddata"; continue }
    $url = "https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/$TessdataRef/$model.traineddata"
    Write-Host "downloading $model.traineddata ..."
    Invoke-WebRequest -Uri $url -OutFile $out -UseBasicParsing
}

# The stock configs directory doesn't come with the traineddata downloads,
# and the app asks for the "tsv" config (word boxes for the text layer) —
# without it Tesseract silently falls back to plain text and OCR produces an
# empty layer. The config is one parameter; write it here.
$configs = Join-Path $tessdata "configs"
New-Item -ItemType Directory -Force $configs | Out-Null
Set-Content -Path (Join-Path $configs "tsv") -Value "tessedit_create_tsv 1" -Encoding ascii

$exe = Join-Path $dest "tesseract.exe"
$env:TESSDATA_PREFIX = $tessdata
& $exe --list-langs
if ($LASTEXITCODE -ne 0) { throw "Staged tesseract failed its smoke test." }
Write-Host "Tesseract staged into $dest"
