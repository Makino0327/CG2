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

# クリア画面の見出し・項目名・ランク・数値を、別の透過画像へまとめる。
$clearStatsPath = Join-Path $resultUiDirectory 'clear_stats.png'
$clearStatsBitmap = [System.Drawing.Bitmap]::new(1024, 688)
$clearStatsGraphics = [System.Drawing.Graphics]::FromImage($clearStatsBitmap)
$clearTitleFont = [System.Drawing.Font]::new('Meiryo', 64, 'Bold', 'Pixel')
$clearLabelFont = [System.Drawing.Font]::new('Meiryo', 28, 'Bold', 'Pixel')
$clearRankFont = [System.Drawing.Font]::new('Bahnschrift', 174, 'Bold', 'Pixel')
$clearDigitFont = [System.Drawing.Font]::new('Bahnschrift', 46, 'Bold', 'Pixel')
$clearStatsFormat = [System.Drawing.StringFormat]::new([System.Drawing.StringFormatFlags]::NoWrap)
$clearBrush = [System.Drawing.Brushes]::White
try {
    $clearStatsGraphics.Clear([System.Drawing.Color]::Transparent)
    $clearStatsGraphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $clearStatsFormat.LineAlignment = [System.Drawing.StringAlignment]::Center
    $clearStatsGraphics.DrawString('ボス撃破', $clearTitleFont, $clearBrush,
        [System.Drawing.RectangleF]::new(0, 0, 480, 96), $clearStatsFormat)

    # 項目名は左揃えで描き、ノーダメージだけ右の数値に揃える。
    $statLabels = @('クリアタイム', 'ボス戦タイム', '敵撃破数', '命中率', '被弾回数')
    for ($row = 0; $row -lt $statLabels.Count; $row++) {
        $rect = [System.Drawing.RectangleF]::new(0, 104 + 48 * $row, 304, 48)
        $clearStatsGraphics.DrawString($statLabels[$row], $clearLabelFont, $clearBrush, $rect, $clearStatsFormat)
    }
    $clearStatsFormat.Alignment = [System.Drawing.StringAlignment]::Far
    $clearStatsGraphics.DrawString('ノーダメージ', $clearLabelFont, $clearBrush,
        [System.Drawing.RectangleF]::new(0, 344, 304, 48), $clearStatsFormat)
    $clearStatsFormat.Alignment = [System.Drawing.StringAlignment]::Center
    $clearStatsGraphics.DrawString('ランク', $clearLabelFont, $clearBrush,
        [System.Drawing.RectangleF]::new(480, 0, 224, 64), $clearStatsFormat)

    # ランクは224ピクセル、数字と記号は48ピクセルごとに切り出せるようにする。
    $rankLetters = 'SABC'
    for ($index = 0; $index -lt $rankLetters.Length; $index++) {
        $rect = [System.Drawing.RectangleF]::new(224 * $index, 400, 224, 224)
        $clearStatsGraphics.DrawString([string]$rankLetters[$index], $clearRankFont, $clearBrush, $rect, $clearStatsFormat)
    }
    $valueGlyphs = '0123456789:%-'
    for ($index = 0; $index -lt $valueGlyphs.Length; $index++) {
        $rect = [System.Drawing.RectangleF]::new(48 * $index, 624, 48, 64)
        $clearStatsGraphics.DrawString([string]$valueGlyphs[$index], $clearDigitFont, $clearBrush, $rect, $clearStatsFormat)
    }
    $clearStatsBitmap.Save($clearStatsPath, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output $clearStatsPath
} finally {
    $clearStatsFormat.Dispose()
    $clearDigitFont.Dispose()
    $clearRankFont.Dispose()
    $clearLabelFont.Dispose()
    $clearTitleFont.Dispose()
    $clearStatsGraphics.Dispose()
    $clearStatsBitmap.Dispose()
}
