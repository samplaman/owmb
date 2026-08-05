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

    # Packaging for dist
    Remove-Item -Path "dist" -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path "dist\OWMB-Windows-11-x64" -Force | Out-Null
    Get-ChildItem -Path "build" -Recurse -Directory -Filter "OWMB.vst3" | Where-Object { $_.FullName -match 'Release' } | ForEach-Object { Copy-Item -Path $_.FullName -Destination "dist\OWMB-Windows-11-x64" -Recurse -Force }
    Get-ChildItem -Path "build" -Recurse -File -Filter "OWMB.exe" | Where-Object { $_.FullName -match 'Release' } | ForEach-Object { Copy-Item -Path $_.FullName -Destination "dist\OWMB-Windows-11-x64\OWMB.exe" -Force }
    Get-ChildItem -Path "build" -Recurse -File -Filter "OpenWav Media Browser.exe" | Where-Object { $_.FullName -match 'Release' } | ForEach-Object { Copy-Item -Path $_.FullName -Destination "dist\OWMB-Windows-11-x64\OWMB.exe" -Force }

    if (Test-Path "dist\OWMB-Windows-11-x64\OWMB.exe") {
        Copy-Item -Path "dist\OWMB-Windows-11-x64\OWMB.exe" -Destination "OWMB-MicrosoftStore-Standalone.exe" -Force
        Copy-Item -Path "dist\OWMB-Windows-11-x64\OWMB.exe" -Destination "OWMB.exe" -Force
    }

    $iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
    if (-not $iscc -and (Test-Path "C:\Program Files (x86)\Inno Setup 6\ISCC.exe")) {
        $iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    }

    if ($iscc) {
        Write-Host "Building Windows Installer via Inno Setup..." -ForegroundColor Cyan
        & $iscc installer.iss
        if (Test-Path "OWMB-Installer.exe") {
            Copy-Item -Path "OWMB-Installer.exe" -Destination "OWMB-MicrosoftStore-Installer.exe" -Force
            Write-Host "Installer Executable: OWMB-Installer.exe" -ForegroundColor Green
            Write-Host "Microsoft Store Installer: OWMB-MicrosoftStore-Installer.exe" -ForegroundColor Green
        } else {
            Write-Host "ERROR: OWMB-Installer.exe was not created by Inno Setup." -ForegroundColor Red
        }
    } else {
        Write-Host "Note: Inno Setup (ISCC.exe) not found. Skipping installer creation." -ForegroundColor Yellow
    }

    # 4. Building MSIX Package
    $makeappx = (Get-Command makeappx -ErrorAction SilentlyContinue).Source
    if (-not $makeappx) {
        $sdkPaths = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -ErrorAction SilentlyContinue | Where-Object { $_.PSIsContainer }
        foreach ($sdk in $sdkPaths) {
            $candidate = Join-Path $sdk.FullName "x64\makeappx.exe"
            if (Test-Path $candidate) {
                $makeappx = $candidate
                break
            }
        }
    }

    $signtool = (Get-Command signtool -ErrorAction SilentlyContinue).Source
    if (-not $signtool) {
        $sdkPaths = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -ErrorAction SilentlyContinue | Where-Object { $_.PSIsContainer }
        foreach ($sdk in $sdkPaths) {
            $candidate = Join-Path $sdk.FullName "x64\signtool.exe"
            if (Test-Path $candidate) {
                $signtool = $candidate
                break
            }
        }
    }

    if ($makeappx -and (Test-Path "dist\OWMB-Windows-11-x64\OWMB.exe")) {
        Write-Host "Building MSIX Package via makeappx.exe..." -ForegroundColor Cyan

        if (-not (Test-Path "msix\Assets\Square44x44Logo.png")) {
            & powershell -ExecutionPolicy Bypass -File "msix\generate_assets.ps1"
        }

        $msixLayout = "dist\OWMB-MSIX-Layout"
        Remove-Item -Path $msixLayout -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Path $msixLayout -Force | Out-Null

        Copy-Item -Path "dist\OWMB-Windows-11-x64\OWMB.exe" -Destination "$msixLayout\OWMB.exe" -Force
        Copy-Item -Path "msix\AppxManifest.xml" -Destination "$msixLayout\AppxManifest.xml" -Force
        Copy-Item -Path "msix\Assets" -Destination "$msixLayout\Assets" -Recurse -Force

        & $makeappx pack /d $msixLayout /p "OWMB.msix" /o
        if (Test-Path "OWMB.msix") {
            Write-Host "MSIX Package Created: OWMB.msix" -ForegroundColor Green

            if ($signtool) {
                Write-Host "Signing MSIX Package..." -ForegroundColor Cyan
                $cert = Get-ChildItem Cert:\CurrentUser\My -ErrorAction SilentlyContinue | Where-Object { $_.Subject -match "CN=OWMB" } | Select-Object -First 1
                if (-not $cert) {
                    Write-Host "Creating self-signed developer certificate (CN=OWMB)..." -ForegroundColor Yellow
                    $cert = New-SelfSignedCertificate -Type Custom -Subject "CN=OWMB" -KeyUsage DigitalSignature -FriendlyName "OWMB Dev Cert" -CertStoreLocation "Cert:\CurrentUser\My"
                }
                if ($cert) {
                    & $signtool sign /fd SHA256 /sha1 $cert.Thumbprint "OWMB.msix"
                    if ($LASTEXITCODE -eq 0) {
                        Write-Host "MSIX Package Signed Successfully!" -ForegroundColor Green
                    } else {
                        Write-Host "Warning: MSIX signing failed." -ForegroundColor Yellow
                    }
                }
            } else {
                Write-Host "Note: signtool.exe not found. MSIX created unsigned." -ForegroundColor Yellow
            }
        } else {
            Write-Host "ERROR: OWMB.msix was not created." -ForegroundColor Red
        }
    } else {
        if (-not $makeappx) {
            Write-Host "Note: makeappx.exe not found. Skipping MSIX package creation." -ForegroundColor Yellow
        }
    }

    Write-Host "Standalone Executable: OWMB-MicrosoftStore-Standalone.exe" -ForegroundColor Yellow
} else {
    Write-Host "Build failed during compilation." -ForegroundColor Red
}

