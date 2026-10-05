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
$build = Join-Path $script:CraftVRoot 'build'

& $cmake -S $script:CraftVRoot -B $build -G 'Visual Studio 17 2022' -A x64 | Out-Host
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
& $cmake --build $build --config $Config --parallel | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$golden = Join-Path $script:CraftVRoot 'protocol\golden\golden_vectors.txt'
if (-not (Test-Path $golden)) {
    Write-Host "Generating $golden (first build)"
    New-Item -ItemType Directory -Force (Split-Path $golden) | Out-Null
    & (Join-Path $build "protocol\cpp\$Config\craftv_golden_gen.exe") $golden | Out-Host
}

if (-not $NoTests) {
    & (Join-Path $build "protocol\cpp\$Config\craftv_link_tests.exe") | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "C++ link tests failed" }
    & (Join-Path $build "rdr2\$Config\craftv_rdr2_tests.exe") | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "RDR2 plugin tests failed" }
}
$asi = Join-Path $build "rdr2\$Config\CraftV.asi"
if (Test-Path $asi) { Write-Host "CraftV.asi: $asi" } else { Write-Host "CraftV.asi not built (ScriptHookRDR2 SDK missing from sdk\ScriptHookRDR2_SDK)" }
Write-Host "mockhost: $(Join-Path $build "tools\mockhost\$Config\mockhost.exe")"
