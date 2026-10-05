# Runs every automated Phase 1 test:
#   1. C++: build, then unit + 1M-record stress tests (craftv_link_tests)
#   2. Java: unit + 1M-record stress tests (gradlew test)
#   3. Chaos + cross-process stress: real mockhost.exe vs the Java headless guest (gradlew integrationTest)
. "$PSScriptRoot\env.ps1"

$root = $script:CraftVRoot
Write-Host "== 1/3 C++ build + tests" -ForegroundColor Cyan
& "$PSScriptRoot\build-native.ps1"

$null = Use-Jdk25
$mock = Join-Path $root 'build\tools\mockhost\Release\mockhost.exe'
Push-Location (Join-Path $root 'fabric')
try {
    Write-Host "== 2/3 Java unit + stress tests" -ForegroundColor Cyan
    & .\gradlew.bat --console=plain test
    if ($LASTEXITCODE -ne 0) { throw "Java tests failed" }
    Write-Host "== 3/3 Chaos + cross-process stress tests" -ForegroundColor Cyan
    & .\gradlew.bat --console=plain integrationTest "-Pmockhost=$mock"
    if ($LASTEXITCODE -ne 0) { throw "Integration tests failed" }
} finally {
    Pop-Location
}
Write-Host "ALL TESTS PASSED" -ForegroundColor Green
