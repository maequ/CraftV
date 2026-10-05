# Phase 1 dev loop: starts the mock host in its own console window (type commands there), then the
# Minecraft dev client with the CraftV mod in this window. Logs go to <repo>\logs.
#   .\scripts\dev-run.ps1                 normal
#   .\scripts\dev-run.ps1 -NoMock         only Minecraft (start a host yourself)
#   .\scripts\dev-run.ps1 -MockArgs '--radius 10 --speed 2'
param(
    [switch]$NoMock,
    [string]$MockArgs = ''
)
. "$PSScriptRoot\env.ps1"

$root = $script:CraftVRoot
$logs = Join-Path $root 'logs'
New-Item -ItemType Directory -Force $logs | Out-Null
$mock = Join-Path $root 'build\tools\mockhost\Release\mockhost.exe'

if (-not $NoMock) {
    if (-not (Test-Path $mock)) {
        Write-Host "mockhost.exe not built yet; building the native parts first..."
        & "$PSScriptRoot\build-native.ps1" -NoTests
    }
    $mockLog = Join-Path $logs 'mockhost.log'
    Write-Host "Starting the mock host in a new window (log: $mockLog)"
    Start-Process -FilePath $mock -ArgumentList "--log `"$mockLog`" $MockArgs" -WorkingDirectory $root
}

$jdk = Use-Jdk25
Write-Host "JDK: $jdk"
$env:CRAFTV_LOG_DIR = $logs
Push-Location (Join-Path $root 'fabric')
try {
    Write-Host "Starting Minecraft (gradlew runClient). The 'CraftV Dev' world opens by itself."
    & .\gradlew.bat runClient
} finally {
    Pop-Location
}
