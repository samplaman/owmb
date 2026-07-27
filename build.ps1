# OpenWav CMake & Kit Auto-Build Script for Windows

Write-Host "Checking C++ Developer Build Environment..." -ForegroundColor Cyan

# 1. Check for cmake in PATH, standard locations, or VS installation
$cmakePath = (Get-Command cmake -ErrorAction SilentlyContinue).Source

if (-not $cmakePath) {
    $commonPaths = @(
        "${env:ProgramFiles}\CMake\bin\cmake.exe",
        "${env:ProgramFiles(x86)}\CMake\bin\cmake.exe",
        "${env:LocalAppData}\Programs\CMake\bin\cmake.exe",
        "C:\ProgramData\chocolatey\bin\cmake.exe"
    )
    foreach ($p in $commonPaths) {
        if (Test-Path $p) {
            $cmakePath = $p
            break
        }
    }
}

if (-not $cmakePath) {
    $vsWhereLocations = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
    )
    foreach ($vsWhere in $vsWhereLocations) {
        if (Test-Path $vsWhere) {
            $vsPath = & $vsWhere -latest -property installationPath
            if ($vsPath) {
                $candidate = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
                if (Test-Path $candidate) {
                    $cmakePath = $candidate
                    break
                }
            }
        }
    }
}

if (-not $cmakePath) {
    Write-Host "ERROR: CMake was not found on your system." -ForegroundColor Red
    Write-Host "You can easily install CMake and C++ Build Tools using winget:" -ForegroundColor Yellow
    Write-Host "  winget install --id Kitware.CMake -e" -ForegroundColor Cyan
    Write-Host "  winget install --id Microsoft.VisualStudio.2022.BuildTools --override '--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended' -e" -ForegroundColor Cyan
    exit 1
}

Write-Host "Using CMake from: $cmakePath" -ForegroundColor Green

# 1.5 Load MSVC developer environment if available
$vcvarsPaths = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
)
foreach ($vcvars in $vcvarsPaths) {
    if (Test-Path $vcvars) {
        Write-Host "Loading MSVC environment from: $vcvars" -ForegroundColor Cyan
        cmd /c "`"$vcvars`" x64 && set" | ForEach-Object {
            if ($_ -match '^(.*?)=(.*)$') {
                Set-Item -Path "env:\$($matches[1])" -Value $matches[2]
            }
        }
        break
    }
}

# 2. Configure build directory
Write-Host "Configuring CMake build directory..." -ForegroundColor Cyan
& $cmakePath -B build -S . -DCMAKE_BUILD_TYPE=Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

# 3. Build target
Write-Host "Compiling OpenWav (VST3 & Standalone)..." -ForegroundColor Cyan
& $cmakePath --build build --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build Succeeded!" -ForegroundColor Green
    Write-Host "VST3 Plugin: build\OpenWav_artefacts\Release\VST3\OpenWav Media Browser.vst3" -ForegroundColor Yellow
    Write-Host "Standalone App: build\OpenWav_artefacts\Release\Standalone\OpenWav Media Browser.exe" -ForegroundColor Yellow
} else {
    Write-Host "Build failed during compilation." -ForegroundColor Red
}
