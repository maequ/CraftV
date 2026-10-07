# Assembles dist\CraftV: what Sary copies into the GTA V folder (gta\INSTALL.txt says how). Only CraftV's own
# files and ReShade's CC0 shader headers go in; ReShade itself and Script Hook V come from their official sites.
#   .\scripts\make-dist.ps1
. "$PSScriptRoot\env.ps1"

$root = $script:CraftVRoot
$dist = Join-Path $root 'dist\CraftV'
$asi = Join-Path $root 'build\gta\Release\CraftV.asi'
if (-not (Test-Path $asi)) { throw "build CraftV.asi first: .\scripts\build-native.ps1" }
$fxh = Join-Path $root 'sdk\reshade-shaders-src\Shaders'
if (-not (Test-Path (Join-Path $fxh 'ReShade.fxh'))) { throw "ReShade.fxh missing: clone crosire/reshade-shaders (slim) into sdk\reshade-shaders-src" }

New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item $asi, (Join-Path $root 'gta\CraftV.ini'), (Join-Path $root 'gta\INSTALL.txt') $dist -Force
$view = Join-Path $dist 'Minecraft view (ReShade)'
$shaders = Join-Path $view 'reshade-shaders\Shaders'
New-Item -ItemType Directory -Force $shaders | Out-Null
Copy-Item (Join-Path $root 'gta\reshade\ReShade.ini'), (Join-Path $root 'gta\reshade\ReShadePreset.ini') $view -Force
Copy-Item (Join-Path $root 'gta\shaders\CraftV.fx'), (Join-Path $fxh 'ReShade.fxh'), (Join-Path $fxh 'ReShadeUI.fxh') $shaders -Force

# A friend who plays through their own GTA (DECISIONS D-030) also gets the Minecraft mod and FRIEND.txt.
$jar = Get-ChildItem (Join-Path $root 'fabric\build\libs') -Filter 'craftv-*.jar' -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notmatch 'sources' } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $jar) { throw "build the mod first: cd fabric; .\gradlew.bat build" }
$old = Join-Path $dist 'For your friend (Minecraft mod)'
if (Test-Path $old) { Remove-Item $old -Recurse -Force }
$mod = Join-Path $dist 'Minecraft mod'
New-Item -ItemType Directory -Force $mod | Out-Null
Get-ChildItem $mod -Filter 'craftv-*.jar' | Remove-Item -Force
Copy-Item $jar.FullName $mod -Force
$friend = Join-Path $mod 'For a friend'
New-Item -ItemType Directory -Force $friend | Out-Null
Copy-Item (Join-Path $root "gta/friend-config/config") $friend -Recurse -Force
Copy-Item (Join-Path $root 'gta\FRIEND.txt') $dist -Force
Get-ChildItem $dist -Recurse -File | ForEach-Object { "{0,-60} {1,10:N0} bytes  {2:HH:mm}" -f $_.FullName.Substring($dist.Length + 1), $_.Length, $_.LastWriteTime }
