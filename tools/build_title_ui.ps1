# タイトル文字を透過PNGにして、実行時の日本語フォントへの依存をなくす
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$titleDirectory = Join-Path $PSScriptRoot '../project/Resources/title'
New-Item -ItemType Directory -Force -Path $titleDirectory | Out-Null
$titlePath = [System.IO.Path]::GetFullPath((Join-Path $titleDirectory 'title_logo.png'))

# 表示サイズの2倍で作成し、ゲーム内で縮小して文字の輪郭を滑らかにする
$bitmap = [System.Drawing.Bitmap]::new(1280, 208)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$titleFont = [System.Drawing.Font]::new('Meiryo', 62, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$subtitleFont = [System.Drawing.Font]::new('Bahnschrift', 14, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$subtitleBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 190, 190, 190))
$format = [System.Drawing.StringFormat]::GenericTypographic.Clone()
$titleOutline = [System.Drawing.Drawing2D.GraphicsPath]::new()
$titleTransform = [System.Drawing.Drawing2D.Matrix]::new()
try {
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.ScaleTransform(2, 2)
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $format.Alignment = [System.Drawing.StringAlignment]::Center
    $format.LineAlignment = [System.Drawing.StringAlignment]::Center
    $format.FormatFlags = $format.FormatFlags -bor [System.Drawing.StringFormatFlags]::NoWrap

    # 白黒の既存UIに合わせ、日本語を主役にして英字は控えめに添える
    # フォントの行間ではなく文字の輪郭で中央を求め、日本語の上下切れを防ぐ
    $titleOutline.AddString('ゾンビリブート', $titleFont.FontFamily, [int]$titleFont.Style,
        $titleFont.Size, [System.Drawing.PointF]::new(0, 0), [System.Drawing.StringFormat]::GenericTypographic)
    $bounds = $titleOutline.GetBounds()
    $titleTransform.Translate(320 - $bounds.X - $bounds.Width / 2, 39 - $bounds.Y - $bounds.Height / 2)
    $titleOutline.Transform($titleTransform)
    $graphics.FillPath([System.Drawing.Brushes]::White, $titleOutline)
    $graphics.DrawString('Z O M B I E   R E B O O T', $subtitleFont, $subtitleBrush,
        [System.Drawing.RectangleF]::new(0, 78, 640, 22), $format)

    $bitmap.Save($titlePath, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output $titlePath
} finally {
    $titleTransform.Dispose()
    $titleOutline.Dispose()
    $format.Dispose()
    $subtitleBrush.Dispose()
    $subtitleFont.Dispose()
    $titleFont.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}
