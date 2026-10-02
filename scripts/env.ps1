# Shared helpers for the RedCraft scripts. Dot-source it: . "$PSScriptRoot\env.ps1"
#
# Finds the tools RedCraft needs without hard-coding machine paths:
#   JDK 25    REDCRAFT_JAVA_HOME, then JAVA_HOME (if it is 25+), then the Java 25 runtime that the
#             official Minecraft Launcher installs (java-runtime-epsilon, which includes javac).
#   CMake     on PATH, then the copy bundled with Visual Studio 2022 (any edition / Build Tools).
$ErrorActionPreference = 'Stop'

$script:RedCraftRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Get-JavaMajor([string]$javaHome) {
    $release = Join-Path $javaHome 'release'
    if (-not (Test-Path $release)) { return 0 }
    $line = Select-String -Path $release -Pattern '^JAVA_VERSION="(\d+)' | Select-Object -First 1
    if ($line) { return [int]$line.Matches[0].Groups[1].Value }
    return 0
}

function Find-Jdk25 {
    $candidates = @()
    if ($env:REDCRAFT_JAVA_HOME) { $candidates += $env:REDCRAFT_JAVA_HOME }
    if ($env:JAVA_HOME) { $candidates += $env:JAVA_HOME }
    $launcherRuntimes = @(
        "$env:LOCALAPPDATA\Packages\Microsoft.4297127D64EC6_8wekyb3d8bbwe\LocalCache\Local\runtime\java-runtime-epsilon\windows-x64\java-runtime-epsilon",
        "$env:APPDATA\.minecraft\runtime\java-runtime-epsilon\windows-x64\java-runtime-epsilon",
        "$env:APPDATA\.minecraft\runtime\java-runtime-epsilon\windows\java-runtime-epsilon"
    )
    $candidates += $launcherRuntimes
    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c 'bin\javac.exe')) -and ((Get-JavaMajor $c) -ge 25)) {
            return (Resolve-Path $c).Path
        }
    }
    throw "No JDK 25 found. Install one (for example: winget install EclipseAdoptium.Temurin.25.JDK) or set REDCRAFT_JAVA_HOME."
}

function Find-CMake {
    $onPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        foreach ($vs in (& $vswhere -all -products * -property installationPath)) {
            $c = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path $c) { return $c }
        }
    }
    throw "CMake not found. Install the 'C++ CMake tools for Windows' component of Visual Studio 2022."
}

function Use-Jdk25 {
    $jdk = Find-Jdk25
    $env:JAVA_HOME = $jdk
    if (-not ($env:Path -like "$jdk\bin;*")) { $env:Path = "$jdk\bin;$env:Path" }
    return $jdk
}
