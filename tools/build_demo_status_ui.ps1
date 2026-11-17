# デモプレイ中の日本語ラベルを、右上で読みやすい画像として生成する。
# 起動直後のデモでもF6で始めたデモでも、同じラベルで解除方法を案内する。
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$demoDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../project/Resources/hud'))
New-Item -ItemType Directory -Force -Path $demoDirectory | Out-Null
$width = 512
$height = 160
$bitmap = [Drawing.Bitmap]::new($width, $height)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$family = [Drawing.FontFamily]::new('Meiryo')
$background = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(220, 8, 16, 13))
$foreground = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(255, 160, 255, 185))
$hint = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(255, 215, 225, 218))
$divider = [Drawing.Pen]::new([Drawing.Color]::FromArgb(120, 160, 255, 185), 2)

# 輪郭を指定した範囲の中央にそろえ、日本語フォントの余白による位置ずれを防ぐ。
function Draw-CenteredText([string]$text, [float]$size, [float]$centerY, [Drawing.Brush]$brush) {
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $transform = [Drawing.Drawing2D.Matrix]::new()
    try {
        $path.AddString($text, $family, [int][Drawing.FontStyle]::Bold, $size,
            [Drawing.PointF]::new(0, 0), [Drawing.StringFormat]::GenericTypographic)
        $bounds = $path.GetBounds()
        $transform.Translate(($width - $bounds.Width) / 2 - $bounds.X, $centerY - $bounds.Height / 2 - $bounds.Y)
        $path.Transform($transform)
        $graphics.FillPath($brush, $path)
    } finally {
        $transform.Dispose()
        $path.Dispose()
    }
}

try {
    $graphics.Clear([Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.FillRectangle($background, 0, 0, $width, $height)
    Draw-CenteredText 'デモプレイ中' 54 50 $foreground
    $graphics.DrawLine($divider, 56, 98, $width - 56, 98)
    Draw-CenteredText 'F6で解除' 36 128 $hint
    $bitmap.Save((Join-Path $demoDirectory 'demo_status.png'), [Drawing.Imaging.ImageFormat]::Png)
} finally {
    $divider.Dispose()
    $hint.Dispose()
    $foreground.Dispose()
    $background.Dispose()
    $family.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}
