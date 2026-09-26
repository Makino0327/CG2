# クリア画面・ゲームオーバー画面の文字を通常のSpriteで使える透過PNGへ描画する
Add-Type -AssemblyName System.Drawing
$resultUiDirectory = Join-Path $PSScriptRoot '../project/Resources/result'
New-Item -ItemType Directory -Force -Path $resultUiDirectory | Out-Null
$resultUiPath = [System.IO.Path]::GetFullPath((Join-Path $resultUiDirectory 'result_labels.png'))
# 1024x320: 上から 見出し(128px)×2行、ボタン文字(64px)を左右に2つ
$resultUiBitmap = [System.Drawing.Bitmap]::new(1024, 320)
$resultUiGraphics = [System.Drawing.Graphics]::FromImage($resultUiBitmap)
$headlineFont = [System.Drawing.Font]::new('Meiryo', 96, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$buttonFont = [System.Drawing.Font]::new('Meiryo', 34, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$resultUiFormat = [System.Drawing.StringFormat]::new()
try {
    $resultUiGraphics.Clear([System.Drawing.Color]::Transparent)
    $resultUiGraphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $resultUiFormat.Alignment = [System.Drawing.StringAlignment]::Center
    $resultUiFormat.LineAlignment = [System.Drawing.StringAlignment]::Center

    # 見出し: 0行目 STAGE CLEAR, 1行目 GAME OVER
    $headlines = @('STAGE CLEAR', 'GAME OVER')
    for ($index = 0; $index -lt $headlines.Count; $index++) {
        $rect = [System.Drawing.RectangleF]::new(0, $index * 128, 1024, 128)
        $resultUiGraphics.DrawString($headlines[$index], $headlineFont,
            [System.Drawing.Brushes]::White, $rect, $resultUiFormat)
    }

    # ボタン文字: y=256 の行に 左 タイトルへ戻る / 右 リスタート
    $buttons = @('タイトルへ戻る', 'リスタート')
    for ($index = 0; $index -lt $buttons.Count; $index++) {
        $rect = [System.Drawing.RectangleF]::new($index * 512, 256, 512, 64)
        $resultUiGraphics.DrawString($buttons[$index], $buttonFont,
            [System.Drawing.Brushes]::White, $rect, $resultUiFormat)
    }

    $resultUiBitmap.Save($resultUiPath, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output $resultUiPath
} finally {
    $resultUiFormat.Dispose()
    $buttonFont.Dispose()
    $headlineFont.Dispose()
    $resultUiGraphics.Dispose()
    $resultUiBitmap.Dispose()
}
