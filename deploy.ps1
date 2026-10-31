param(
    [string]$BuildDir = "$PSScriptRoot/build-release",
    [string]$OutputDir = "$PSScriptRoot/dist",
    [string]$QtBin = "C:/Qt/6.9.3/msvc2022_64/bin"
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $BuildDir 'PaintStation.exe'
$deployQt = Join-Path $QtBin 'windeployqt.exe'
if (!(Test-Path -LiteralPath $exe) -or !(Test-Path -LiteralPath $deployQt)) { throw 'Release executable or windeployqt not found.' }
if ((Test-Path -LiteralPath $OutputDir) -and (Get-ChildItem -LiteralPath $OutputDir -Force | Select-Object -First 1)) { throw 'Output directory must be empty.' }
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
Copy-Item -LiteralPath $exe -Destination $OutputDir -Force
foreach ($name in @('EftSdk.dll','log4cpp.dll','rlibcpp-bcc.dll','rlibcpp-tool.dll','log4cpp.conf','realsense2.dll','opencv_world4120.dll')) {
    $source = Join-Path $BuildDir $name
    if (!(Test-Path -LiteralPath $source)) { throw "Missing runtime file: $source" }
    Copy-Item -LiteralPath $source -Destination $OutputDir -Force
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'resources') -Destination $OutputDir -Recurse -Force
& $deployQt --release --no-compiler-runtime (Join-Path $OutputDir 'PaintStation.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed.' }
Write-Host 'Deploy complete. Install the official Visual C++ x64 Redistributable and test on a clean PC.'
