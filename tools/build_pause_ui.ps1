# ポーズ画面の文字を透過画像へ描画し、ゲーム側のフォント依存をなくす。
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$pauseDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../project/Resources/hud'))
New-Item -ItemType Directory -Force -Path $pauseDirectory | Out-Null
$pauseFamily = [System.Drawing.FontFamily]::new('Meiryo')

function Draw-HudText($graphics, [string]$text, [float]$size, [float]$x, [float]$y,
    [float]$height, [System.Drawing.Color]$color, [bool]$bold = $false, [float]$centerWidth = 0) {
    # 日本語の輪郭を基準に配置して、フォントの大きな行間による文字切れを防ぐ
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $transform = [System.Drawing.Drawing2D.Matrix]::new()
    $brush = [System.Drawing.SolidBrush]::new($color)
    try {
        $style = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }
        $path.AddString($text, $pauseFamily, [int]$style, $size,
            [System.Drawing.PointF]::new(0, 0), [System.Drawing.StringFormat]::GenericTypographic)
        $bounds = $path.GetBounds()
        $offsetX = if ($centerWidth -gt 0) { ($centerWidth - $bounds.Width) / 2 } else { 0 }
        $transform.Translate($x + $offsetX - $bounds.X, $y + ($height - $bounds.Height) / 2 - $bounds.Y)
        $path.Transform($transform)
        $graphics.FillPath($brush, $path)
    } finally {
        $brush.Dispose()
        $transform.Dispose()
        $path.Dispose()
    }
}

function Save-HudImage([string]$name, [int]$width, [int]$height, [scriptblock]$draw) {
    $bitmap = [System.Drawing.Bitmap]::new($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        & $draw $graphics
        $path = Join-Path $pauseDirectory $name
        $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Output $path
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

try {
    # 1200×128の各行を半分の大きさで表示し、日本語も読みやすくする。
    Save-HudImage 'pause_labels.png' 1200 384 {
        param($graphics)
        $labels = @('PAUSED', 'ゲームに戻る', 'タイトルに戻る')
        $sizes = @(104, 44, 44)
        for ($row = 0; $row -lt $labels.Count; $row++) {
            Draw-HudText $graphics $labels[$row] $sizes[$row] 0 ($row * 128) 128 ([Drawing.Color]::White) ($row -lt 3)
        }
    }
} finally {
    $pauseFamily.Dispose()
}
