$ErrorActionPreference = "Stop"
$repository = "TheROMZ52/MiniCraft3D"
$target = Join-Path $PSScriptRoot "MiniCraft3D.exe"
$temp = Join-Path $PSScriptRoot "MiniCraft3D.update.exe"
$backup = Join-Path $PSScriptRoot "MiniCraft3D.exe.backup"
$textureDirectory = Join-Path $PSScriptRoot "textures"
$textureTarget = Join-Path $textureDirectory "atlas.ppm"
$textureTemp = Join-Path $textureDirectory "atlas.update.ppm"
$textureBackup = Join-Path $textureDirectory "atlas.ppm.backup"

try {
    Write-Host "Checking the latest MiniCraft3D release..."
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$repository/releases/latest" -Headers @{ "User-Agent" = "MiniCraft3D-Updater" }
    $exeAsset = $release.assets | Where-Object { $_.name -eq "MiniCraft3D.exe" } | Select-Object -First 1
    $textureAsset = $release.assets | Where-Object { $_.name -eq "atlas.ppm" } | Select-Object -First 1
    if (-not $exeAsset -or -not $textureAsset) {
        throw "The latest release is missing the executable or texture atlas."
    }

    New-Item -ItemType Directory -Path $textureDirectory -Force | Out-Null
    Write-Host "Downloading $($release.tag_name)..."
    Invoke-WebRequest -Uri $exeAsset.browser_download_url -OutFile $temp -Headers @{ "User-Agent" = "MiniCraft3D-Updater" }
    Invoke-WebRequest -Uri $textureAsset.browser_download_url -OutFile $textureTemp -Headers @{ "User-Agent" = "MiniCraft3D-Updater" }
    if (-not (Test-Path $temp) -or (Get-Item $temp).Length -lt 20000) {
        throw "The downloaded executable is missing or too small."
    }
    if (-not (Test-Path $textureTemp) -or (Get-Item $textureTemp).Length -lt 1000) {
        throw "The downloaded texture atlas is missing or too small."
    }

    if (Test-Path $target) { Copy-Item $target $backup -Force }
    if (Test-Path $textureTarget) { Copy-Item $textureTarget $textureBackup -Force }
    Move-Item $temp $target -Force
    Move-Item $textureTemp $textureTarget -Force
    Write-Host "Updated to $($release.tag_name)."
    Write-Host "Your worlds and settings were left untouched."
    Write-Host "Previous files are backed up with the .backup extension."
}
catch {
    if (Test-Path $temp) { Remove-Item $temp -Force }
    if (Test-Path $textureTemp) { Remove-Item $textureTemp -Force }
    Write-Error $_
    exit 1
}
