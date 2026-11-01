# デモプレイ中の日本語ラベルを、右上で読みやすい画像として生成する。
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$demoDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../project/Resources/hud'))
New-Item -ItemType Directory -Force -Path $demoDirectory | Out-Null
$bitmap = [Drawing.Bitmap]::new(512, 96)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$family = [Drawing.FontFamily]::new('Meiryo')
$path = [Drawing.Drawing2D.GraphicsPath]::new()
$transform = [Drawing.Drawing2D.Matrix]::new()
$background = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(220, 8, 16, 13))
$foreground = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(255, 160, 255, 185))
try {
    $graphics.Clear([Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.FillRectangle($background, 0, 0, 512, 96)
    # 輪郭を中央にそろえ、日本語フォントの余白による位置ずれを防ぐ。
    $path.AddString('デモプレイ中', $family, [int][Drawing.FontStyle]::Bold, 54,
        [Drawing.PointF]::new(0, 0), [Drawing.StringFormat]::GenericTypographic)
    $bounds = $path.GetBounds()
    $transform.Translate((512 - $bounds.Width) / 2 - $bounds.X, (96 - $bounds.Height) / 2 - $bounds.Y)
    $path.Transform($transform)
    $graphics.FillPath($foreground, $path)
    $bitmap.Save((Join-Path $demoDirectory 'demo_status.png'), [Drawing.Imaging.ImageFormat]::Png)
} finally {
    $foreground.Dispose()
    $background.Dispose()
    $transform.Dispose()
    $path.Dispose()
    $family.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}
