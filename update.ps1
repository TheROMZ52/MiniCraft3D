$ErrorActionPreference = "Stop"
$repository = "TheROMZ52/MiniCraft3D"
$target = Join-Path $PSScriptRoot "MiniCraft3D.exe"
$temp = Join-Path $PSScriptRoot "MiniCraft3D.update.exe"
$backup = Join-Path $PSScriptRoot "MiniCraft3D.exe.backup"

try {
    Write-Host "Checking the latest MiniCraft3D release..."
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$repository/releases/latest" -Headers @{ "User-Agent" = "MiniCraft3D-Updater" }
    $asset = $release.assets | Where-Object { $_.name -eq "MiniCraft3D.exe" } | Select-Object -First 1
    if (-not $asset) {
        throw "The latest release does not contain MiniCraft3D.exe."
    }

    Write-Host "Downloading $($release.tag_name)..."
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $temp -Headers @{ "User-Agent" = "MiniCraft3D-Updater" }
    if (-not (Test-Path $temp) -or (Get-Item $temp).Length -lt 20000) {
        throw "The downloaded file is missing or too small."
    }

    if (Test-Path $target) {
        Copy-Item $target $backup -Force
    }
    Move-Item $temp $target -Force
    Write-Host "Updated to $($release.tag_name)."
    if (Test-Path $backup) {
        Write-Host "Previous executable backup: MiniCraft3D.exe.backup"
    }
}
catch {
    if (Test-Path $temp) {
        Remove-Item $temp -Force
    }
    Write-Error $_
    exit 1
}
