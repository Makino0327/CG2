# DEAD CURRENTのホラー調ロゴと霧を、ゲームで使う透過画像として生成する
param([switch]$Preview)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$titleDirectory = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../project/Resources/title'))
New-Item -ItemType Directory -Force -Path $titleDirectory | Out-Null

# ベクター描画を2倍解像度で保存し、細い文字や傷の輪郭を滑らかにする
function New-Canvas([int]$width, [int]$height) {
    $script:bitmap = [System.Drawing.Bitmap]::new($width * 2, $height * 2)
    $script:graphics = [System.Drawing.Graphics]::FromImage($script:bitmap)
    $script:graphics.Clear([System.Drawing.Color]::Transparent)
    $script:graphics.ScaleTransform(2, 2)
    $script:graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $script:graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
}
function Save-Canvas([string]$path) {
    try { $script:bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png) }
    finally { $script:graphics.Dispose(); $script:bitmap.Dispose() }
}
function Get-Color([string]$hex, [int]$alpha = 255) {
    return [System.Drawing.Color]::FromArgb($alpha, [System.Drawing.ColorTranslator]::FromHtml($hex))
}
function Draw-Rect([float]$x, [float]$y, [float]$w, [float]$h, [string]$hex, [int]$alpha = 255) {
    $brush = [System.Drawing.SolidBrush]::new((Get-Color $hex $alpha))
    try { $script:graphics.FillRectangle($brush, $x, $y, $w, $h) } finally { $brush.Dispose() }
}
function Draw-Line([float]$x1, [float]$y1, [float]$x2, [float]$y2, [string]$hex, [float]$width = 1, [int]$alpha = 255) {
    $pen = [System.Drawing.Pen]::new((Get-Color $hex $alpha), $width)
    try { $script:graphics.DrawLine($pen, $x1, $y1, $x2, $y2) } finally { $pen.Dispose() }
}
function Draw-Text([string]$text, [float]$x, [float]$y, [float]$size, [string]$hex, [string]$family = 'Bahnschrift') {
    $font = [System.Drawing.Font]::new($family, $size, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
    $brush = [System.Drawing.SolidBrush]::new((Get-Color $hex))
    try { $script:graphics.DrawString($text, $font, $brush, $x, $y, [System.Drawing.StringFormat]::GenericTypographic) }
    finally { $font.Dispose(); $brush.Dispose() }
}

# 小さな説明文字も、実際の文字幅を測って画面の中央へ揃える
function Draw-CenteredText([string]$text, [float]$y, [float]$size, [string]$hex, [float]$canvasWidth) {
    $font = [System.Drawing.Font]::new('Meiryo', $size, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
    try {
        $sizeInPixels = $script:graphics.MeasureString($text, $font, [System.Drawing.PointF]::new(0,0), [System.Drawing.StringFormat]::GenericTypographic)
        Draw-Text $text (($canvasWidth-$sizeInPixels.Width)/2) $y $size $hex 'Meiryo'
    } finally { $font.Dispose() }
}

# 文字の輪郭を基準に配置し、フォントの行間による位置ずれを防ぐ
function Draw-LogoWord([string]$text, [float]$x, [float]$y, [float]$width, [float]$height, [string]$hex) {
    $font = [System.Drawing.Font]::new('Arial', 100, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $matrix = [System.Drawing.Drawing2D.Matrix]::new()
    $brush = [System.Drawing.SolidBrush]::new((Get-Color $hex))
    try {
        $path.AddString($text, $font.FontFamily, [int]$font.Style, 100, [System.Drawing.PointF]::new(0, 0), [System.Drawing.StringFormat]::GenericTypographic)
        $bounds = $path.GetBounds()
        $sx = $width / $bounds.Width
        $sy = $height / $bounds.Height
        $matrix.Scale($sx, $sy)
        $matrix.Translate(-$bounds.X + $x / $sx, -$bounds.Y + $y / $sy)
        $path.Transform($matrix)
        $script:graphics.FillPath($brush, $path)
        # 傷は文字の内側だけを抜き、背景が透ける塗装剥がれにする
        $saved = $script:graphics.Save()
        $script:graphics.SetClip($path)
        $script:graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $random = [System.Random]::new(42 + [int]$y)
        for ($i = 0; $i -lt 290; $i++) {
            Draw-Rect ($x + $random.NextDouble() * $width) ($y + $random.NextDouble() * $height) (0.5 + $random.NextDouble() * 3) (0.5 + $random.NextDouble() * 1.2) '#000000' 0
        }
        Draw-Line ($x + $width * 0.64) $y ($x + $width * 0.61) ($y + $height * 0.48) '#000000' 1.5 0
        Draw-Line ($x + $width * 0.61) ($y + $height * 0.48) ($x + $width * 0.66) ($y + $height) '#000000' 1.2 0
        $script:graphics.Restore($saved)
    } finally { $font.Dispose(); $path.Dispose(); $matrix.Dispose(); $brush.Dispose() }
}

# 横長のロゴは、暗赤色と灰白色に分けて傷んだ塗装文字にする
New-Canvas 900 132
Draw-LogoWord 'DEAD' 2 8 283 100 '#9B2525'
Draw-LogoWord 'CURRENT' 310 8 588 100 '#DEDEDA'
Save-Canvas (Join-Path $titleDirectory 'title_logo.png')

# 背景の戦闘を残しつつ、四辺を暗く落とす。透過率は中心から滑らかに変える
New-Canvas 640 360
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
for ($y = 0; $y -lt 360; $y += 2) {
    for ($x = 0; $x -lt 640; $x += 2) {
        $dx = ($x-320.0)/360.0
        $dy = ($y-180.0)/205.0
        $edge = [Math]::Min(1.0, [Math]::Pow($dx*$dx+$dy*$dy, 0.8))
        $opacity = [int](102 + 135*$edge)
        Draw-Rect $x $y 2 2 '#020507' $opacity
    }
}
Save-Canvas (Join-Path $titleDirectory 'title_shade.png')

# 中心から外へ透明になる雲を重ね、境界の出ない薄い霧を作る
New-Canvas 760 150
$mistRandom = [System.Random]::new(182)
for ($cloud = 0; $cloud -lt 24; $cloud++) {
    $cloudPath = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $cloudX = 24 + $mistRandom.Next(520)
    $cloudY = 18 + $mistRandom.Next(30)
    $cloudPath.AddEllipse([float]$cloudX, [float]$cloudY, [float](100 + $mistRandom.Next(100)), [float](50 + $mistRandom.Next(48)))
    $cloudBrush = [System.Drawing.Drawing2D.PathGradientBrush]::new($cloudPath)
    $cloudBrush.CenterColor = Get-Color '#94A4A6' 35
    $cloudBrush.SurroundColors = [System.Drawing.Color[]]@([System.Drawing.Color]::Transparent)
    try { $graphics.FillPath($cloudBrush, $cloudPath) }
    finally { $cloudBrush.Dispose(); $cloudPath.Dispose() }
}
Save-Canvas (Join-Path $titleDirectory 'title_haze.png')

# 本番の画像と同じ座標で、背景の戦闘を省いたUIプレビューを出力する
if ($Preview) {
    New-Canvas 1280 720
    $graphics.Clear((Get-Color '#273137'))
    $logoImage = [System.Drawing.Image]::FromFile((Join-Path $titleDirectory 'title_logo.png'))
    $shadeImage = [System.Drawing.Image]::FromFile((Join-Path $titleDirectory 'title_shade.png'))
    $hazeImage = [System.Drawing.Image]::FromFile((Join-Path $titleDirectory 'title_haze.png'))
    $labelsImage = [System.Drawing.Image]::FromFile((Join-Path $titleDirectory 'menu_labels.png'))
    try {
        $graphics.DrawImage($shadeImage, 0, 0, 1280, 720)
        # 霧の濃さは本番の待機状態と同じ程度に落として確認する
        $mistAttributes = [System.Drawing.Imaging.ImageAttributes]::new()
        $mistMatrix = [System.Drawing.Imaging.ColorMatrix]::new()
        $mistMatrix.Matrix00 = 0.70
        $mistMatrix.Matrix11 = 0.78
        $mistMatrix.Matrix22 = 0.80
        $mistMatrix.Matrix33 = 0.22
        $mistAttributes.SetColorMatrix($mistMatrix)
        try { $graphics.DrawImage($hazeImage, [System.Drawing.Rectangle]::new(-120,350,1520,300), 0,0,$hazeImage.Width,$hazeImage.Height,[System.Drawing.GraphicsUnit]::Pixel,$mistAttributes) }
        finally { $mistAttributes.Dispose() }
        $graphics.DrawImage($logoImage, 190, 170, 900, 132)
        for ($button = 0; $button -lt 2; $button++) {
            $y = 470 + $button * 54
            $labelAttributes = [System.Drawing.Imaging.ImageAttributes]::new()
            $labelMatrix = [System.Drawing.Imaging.ColorMatrix]::new()
            $labelMatrix.Matrix33 = $(if ($button -eq 0) { 1.0 } else { 0.45 })
            $labelAttributes.SetColorMatrix($labelMatrix)
            try { $graphics.DrawImage($labelsImage, [System.Drawing.Rectangle]::new(520, $y+2, 240, 36), 256, $button*64, 512, 64, [System.Drawing.GraphicsUnit]::Pixel, $labelAttributes) }
            finally { $labelAttributes.Dispose() }
        }
        Draw-Line 602 510 678 510 '#B4A19C' 1 160
        Draw-Rect 576 487 3 3 '#9B2525'
    } finally { $logoImage.Dispose(); $shadeImage.Dispose(); $hazeImage.Dispose(); $labelsImage.Dispose() }
    $previewDirectory = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../generated'))
    New-Item -ItemType Directory -Force -Path $previewDirectory | Out-Null
    Save-Canvas (Join-Path $previewDirectory 'dead-current-preview.png')
}
Write-Output 'DEAD CURRENT: title_logo.png / title_shade.png / title_haze.png'
