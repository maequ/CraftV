# Builds the native parts (protocol library, its tests, the mock host) and runs the C++ tests.
#   .\scripts\build-native.ps1            Release build + tests
#   .\scripts\build-native.ps1 -NoTests   build only
#   .\scripts\build-native.ps1 -Config Debug
param(
    [ValidateSet('Release', 'Debug')] [string]$Config = 'Release',
    [switch]$NoTests
)
. "$PSScriptRoot\env.ps1"

$cmake = Find-CMake
$build = Join-Path $script:RedCraftRoot 'build'

& $cmake -S $script:RedCraftRoot -B $build -G 'Visual Studio 17 2022' -A x64 | Out-Host
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
& $cmake --build $build --config $Config --parallel | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$golden = Join-Path $script:RedCraftRoot 'protocol\golden\golden_vectors.txt'
if (-not (Test-Path $golden)) {
    Write-Host "Generating $golden (first build)"
    New-Item -ItemType Directory -Force (Split-Path $golden) | Out-Null
    & (Join-Path $build "protocol\cpp\$Config\redcraft_golden_gen.exe") $golden | Out-Host
}

if (-not $NoTests) {
    & (Join-Path $build "protocol\cpp\$Config\redcraft_link_tests.exe") | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "C++ tests failed" }
}
Write-Host "mockhost: $(Join-Path $build "tools\mockhost\$Config\mockhost.exe")"
