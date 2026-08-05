Add-Type -AssemblyName System.Drawing

$sourcePath = "$PSScriptRoot\..\owmbico.png"
$assetsDir = "$PSScriptRoot\Assets"

if (-not (Test-Path $assetsDir)) {
    New-Item -ItemType Directory -Path $assetsDir -Force | Out-Null
}

function Resize-Image {
    param(
        [string]$TargetFile,
        [int]$Width,
        [int]$Height
    )
    $srcImg = [System.Drawing.Image]::FromFile($sourcePath)
    $bmp = New-Object System.Drawing.Bitmap($Width, $Height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.Clear([System.Drawing.Color]::Transparent)

    $ratioX = $Width / $srcImg.Width
    $ratioY = $Height / $srcImg.Height
    $ratio = [Math]::Min($ratioX, $ratioY)
    $newWidth = [int]($srcImg.Width * $ratio)
    $newHeight = [int]($srcImg.Height * $ratio)
    $posX = [int](($Width - $newWidth) / 2)
    $posY = [int](($Height - $newHeight) / 2)

    $g.DrawImage($srcImg, $posX, $posY, $newWidth, $newHeight)
    $bmp.Save($TargetFile, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
    $srcImg.Dispose()
}

Resize-Image -TargetFile "$assetsDir\Square44x44Logo.png" -Width 44 -Height 44
Resize-Image -TargetFile "$assetsDir\Square150x150Logo.png" -Width 150 -Height 150
Resize-Image -TargetFile "$assetsDir\Wide310x150Logo.png" -Width 310 -Height 150
Resize-Image -TargetFile "$assetsDir\StoreLogo.png" -Width 50 -Height 50

Write-Host "MSIX Visual Assets successfully generated in $assetsDir" -ForegroundColor Green
