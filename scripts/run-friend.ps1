# Starts a stand-in friend: a second dev Minecraft (its own folder, fabric\run-friend) that joins the host
# world the way a player using Multiplayer would. For automated co-op tests on one PC; real friends just
# use their normal Minecraft. The host must be started with $env:CRAFTV_DEV_NO_AUTH = 'true', because
# this test client has no Minecraft account (brief §2.4: never do that for real play).
#   .\scripts\run-friend.ps1                         joins localhost:25565 as CraftVFriend
#   .\scripts\run-friend.ps1 -Bot                    ...and walks, jumps, places and breaks blocks by itself
#   .\scripts\run-friend.ps1 -Join 192.168.1.5:25565 -Name Alex
param(
    [string]$Join = 'localhost:25565',
    [string]$Name = 'CraftVFriend',
    [switch]$Bot
)
. "$PSScriptRoot\env.ps1"

$root = $script:CraftVRoot
$logs = Join-Path $root 'logs'
New-Item -ItemType Directory -Force (Join-Path $logs 'friend') | Out-Null
$null = Use-Jdk25
$env:CRAFTV_LOG_DIR = $logs
$env:CRAFTV_JOIN = $Join
$env:CRAFTV_FRIEND_NAME = $Name
$env:CRAFTV_FRIEND_BOT = if ($Bot) { 'true' } else { 'false' }
Push-Location (Join-Path $root 'fabric')
try {
    Write-Host "Starting the stand-in friend '$Name', joining $Join (log: logs\friend\craftv-fabric.log)"
    & .\gradlew.bat --console=plain runFriend
} finally {
    Pop-Location
}
