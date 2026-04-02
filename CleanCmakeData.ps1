# cmake-purge.ps1 — Remove all CMake cache and generated build artifacts
# Usage: .\cmake-purge.ps1 [directory]
#        Defaults to current directory if no argument is given.

param(
    [string]$Target = "."
)

if (-Not (Test-Path $Target -PathType Container)) {
    Write-Host "Error: '$Target' is not a valid directory." -ForegroundColor Red
    exit 1
}

$resolved = Resolve-Path $Target
Write-Host "Purging CMake cache in: $resolved" -ForegroundColor Cyan
Write-Host "-------------------------------------------"

# Files to remove
$files = @(
    "CMakeCache.txt",
    "cmake_install.cmake",
    "CTestTestfile.cmake",
    "CPackConfig.cmake",
    "CPackSourceConfig.cmake",
    "install_manifest.txt",
    "Makefile",
    # Visual Studio generator files
    "*.sln",
    "*.vcxproj",
    "*.vcxproj.filters",
    "*.vcxproj.user"
)

# Directories to remove
$dirs = @(
    "CMakeFiles",
    "_deps",
    ".cmake",
    # Visual Studio build output folders
    "Debug",
    "Release",
    "RelWithDebInfo",
    "MinSizeRel",
    "x64",
    "x86",
    "*.dir"       # e.g. Raytracer.dir
)

$count = 0

foreach ($f in $files) {
    Get-ChildItem -Path $Target -Filter $f -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "  [FILE] Removing: $($_.FullName)" -ForegroundColor Yellow
        Remove-Item $_.FullName -Force
        $count++
    }
}

foreach ($d in $dirs) {
    Get-ChildItem -Path $Target -Filter $d -Recurse -Force -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "  [DIR]  Removing: $($_.FullName)" -ForegroundColor Yellow
        Remove-Item $_.FullName -Recurse -Force
        $count++
    }
}

Write-Host "-------------------------------------------"
Write-Host "Done. Removed $count item(s) from '$resolved'." -ForegroundColor Green