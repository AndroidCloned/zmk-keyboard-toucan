#Requires -Version 5.1
<#
.SYNOPSIS
  Build all Toucan nightly UF2s via Docker into ../toucan-nightly-firmware/firmware/

.EXAMPLE
  .\scripts\build-nightly.ps1
  .\scripts\build-nightly.ps1 -SkipWestUpdate
#>
[CmdletBinding()]
param(
    [switch] $SkipWestUpdate,
    [string] $OutputDir = "",
    [string] $DockerImage = "zmkfirmware/zmk-build-arm:stable"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $OutputDir) {
    $OutputDir = Join-Path (Split-Path -Parent $RepoRoot) "toucan-nightly-firmware\firmware"
}

function Test-DockerReady {
    try {
        $null = docker info 2>&1
        return $LASTEXITCODE -eq 0
    } catch {
        return $false
    }
}

if (-not (Test-DockerReady)) {
    Write-Error @"
Docker is not running or not installed.
Install Docker Desktop, start it, then re-run:
  .\scripts\build-nightly.ps1

Or push to GitHub and download the 'firmware' artifact from Actions.
"@
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
Write-Host "Repo:   $RepoRoot"
Write-Host "Output: $OutputDir"
Write-Host "Image:  $DockerImage"

Write-Host "`nPulling $DockerImage (if needed)..."
docker pull $DockerImage

$skipVal = if ($SkipWestUpdate) { "1" } else { "0" }

Write-Host "`nStarting build (first run: 30-90 min while west update fetches ZMK)..."
# Docker logs progress on stderr; do not treat that as a terminating error.
$prevEap = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
# Named volume keeps west/Zephyr checkout off the shield repo (CI-style isolation).
docker volume create toucan-zmk-west 2>$null | Out-Null
docker run --rm `
    -e "REPO=/workdir" `
    -e "OUT=/out" `
    -e "SKIP_UPDATE=$skipVal" `
    -e "BASE_DIR=/tmp/zmk-workspace" `
    -v "${RepoRoot}:/workdir" `
    -v "${OutputDir}:/out" `
    -v "toucan-zmk-west:/tmp/zmk-workspace" `
    -w /workdir `
    $DockerImage `
    bash -c "sed -i 's/\r$//' scripts/build-nightly-inner.sh && bash scripts/build-nightly-inner.sh"
$dockerExit = $LASTEXITCODE
$ErrorActionPreference = $prevEap

if ($dockerExit -ne 0) {
    Write-Error "Docker build failed with exit code $dockerExit"
}

Write-Host "`nFirmware written to: $OutputDir"
Get-ChildItem $OutputDir -Filter *.uf2 | Sort-Object Name | Format-Table Name, Length, LastWriteTime -AutoSize
