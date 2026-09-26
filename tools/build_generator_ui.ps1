# 日本語の操作案内を通常のSpriteで使える透過PNGへ描画する
Add-Type -AssemblyName System.Drawing
$generatorUiDirectory = Join-Path $PSScriptRoot '../project/Resources/generator'
$generatorUiPath = [System.IO.Path]::GetFullPath((Join-Path $generatorUiDirectory 'interaction_labels.png'))
$generatorUiBitmap = [System.Drawing.Bitmap]::new(512, 192)
$generatorUiGraphics = [System.Drawing.Graphics]::FromImage($generatorUiBitmap)
$generatorUiFont = [System.Drawing.Font]::new('Meiryo', 32, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$generatorUiFormat = [System.Drawing.StringFormat]::new()
try {
    # 1行64ピクセルで、操作案内・起動中・完了の順に保存する
    $generatorUiGraphics.Clear([System.Drawing.Color]::Transparent)
    $generatorUiGraphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $generatorUiFormat.Alignment = [System.Drawing.StringAlignment]::Center
    $generatorUiFormat.LineAlignment = [System.Drawing.StringAlignment]::Center
    $generatorUiLabels = @('[ E ] で起動', 'ジェネレーター起動中', '起動完了')
    for ($labelIndex = 0; $labelIndex -lt $generatorUiLabels.Count; $labelIndex++) {
        $generatorUiRect = [System.Drawing.RectangleF]::new(0, $labelIndex * 64, 512, 64)
        $generatorUiGraphics.DrawString($generatorUiLabels[$labelIndex], $generatorUiFont,
            [System.Drawing.Brushes]::White, $generatorUiRect, $generatorUiFormat)
    }
    $generatorUiBitmap.Save($generatorUiPath, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output $generatorUiPath
} finally {
    $generatorUiFormat.Dispose()
    $generatorUiFont.Dispose()
    $generatorUiGraphics.Dispose()
    $generatorUiBitmap.Dispose()
}
