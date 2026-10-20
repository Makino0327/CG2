# 作戦開始の文字を透過画像にし、演出中のフォント生成を不要にする。
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$missionDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../project/Resources/hud'))
New-Item -ItemType Directory -Force -Path $missionDirectory | Out-Null
$bitmap = [Drawing.Bitmap]::new(1024, 160)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$family = [Drawing.FontFamily]::new('Arial')
$path = [Drawing.Drawing2D.GraphicsPath]::new()
$transform = [Drawing.Drawing2D.Matrix]::new()
$outline = [Drawing.Pen]::new([Drawing.Color]::FromArgb(190, 4, 7, 8), 5)
try {
    $graphics.Clear([Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $path.AddString('MISSION START', $family, [int][Drawing.FontStyle]::Bold, 96,
        [Drawing.PointF]::new(0, 0), [Drawing.StringFormat]::GenericTypographic)
    $bounds = $path.GetBounds()
    # 文字の実際の輪郭を使い、上下左右の余白を均等にする。
    $transform.Translate((1024 - $bounds.Width) / 2 - $bounds.X, (160 - $bounds.Height) / 2 - $bounds.Y)
    $path.Transform($transform)
    $outline.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    $graphics.DrawPath($outline, $path)
    $graphics.FillPath([Drawing.Brushes]::White, $path)
    $bitmap.Save((Join-Path $missionDirectory 'mission_start.png'), [Drawing.Imaging.ImageFormat]::Png)
} finally {
    $outline.Dispose()
    $transform.Dispose()
    $path.Dispose()
    $family.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}
