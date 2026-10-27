# ゲームオーバーの文字を、背景とは別の透過画像として作る。
Add-Type -AssemblyName System.Drawing
$gameOverDirectory = Join-Path $PSScriptRoot '../project/Resources/gameover'
New-Item -ItemType Directory -Force -Path $gameOverDirectory | Out-Null
$gameOverBitmap = [System.Drawing.Bitmap]::new(1280, 288)
$gameOverGraphics = [System.Drawing.Graphics]::FromImage($gameOverBitmap)
$gameOverHeadingFont = [System.Drawing.Font]::new('Times New Roman', 116,
    [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$gameOverMenuFont = [System.Drawing.Font]::new('Segoe UI', 60,
    [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$gameOverFormat = [System.Drawing.StringFormat]::GenericTypographic.Clone()
$gameOverFormat.FormatFlags = $gameOverFormat.FormatFlags -bor [System.Drawing.StringFormatFlags]::MeasureTrailingSpaces

function Draw-GameOverText {
    param([string]$Text, [System.Drawing.Font]$Font, [float]$Spacing,
        [float]$Top, [float]$Height)
    $textPath = [System.Drawing.Drawing2D.GraphicsPath]::new()
    try {
        $cursor = [single]0
        foreach ($character in $Text.ToCharArray()) {
            $letter = [string]$character
            $textPath.AddString($letter, $Font.FontFamily, [int]$Font.Style, $Font.Size,
                [System.Drawing.PointF]::new($cursor, 0), $gameOverFormat)
            $measured = $gameOverGraphics.MeasureString($letter, $Font,
                [System.Drawing.PointF]::new(0, 0), $gameOverFormat)
            $cursor += $measured.Width + $Spacing
        }
        # 字形そのものの境界で中央揃えし、フォントの余白に左右されないようにする。
        $bounds = $textPath.GetBounds()
        $transform = [System.Drawing.Drawing2D.Matrix]::new()
        try {
            $transform.Translate((1280 - $bounds.Width) * 0.5 - $bounds.X,
                $Top + ($Height - $bounds.Height) * 0.5 - $bounds.Y)
            $textPath.Transform($transform)
            $gameOverGraphics.FillPath([System.Drawing.Brushes]::White, $textPath)
        } finally { $transform.Dispose() }
    } finally { $textPath.Dispose() }
}

try {
    $gameOverGraphics.Clear([System.Drawing.Color]::Transparent)
    $gameOverGraphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    # 見出し160px、メニュー64pxを2行に分け、ゲーム側で個別に切り出す。
    Draw-GameOverText 'YOU ARE DEAD' $gameOverHeadingFont 14 0 160
    Draw-GameOverText 'RETRY' $gameOverMenuFont 14 160 64
    Draw-GameOverText 'RETURN TO TITLE' $gameOverMenuFont 12 224 64
    $gameOverOutput = [System.IO.Path]::GetFullPath((Join-Path $gameOverDirectory 'gameover_labels.png'))
    $gameOverBitmap.Save($gameOverOutput, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output $gameOverOutput
} finally {
    $gameOverFormat.Dispose()
    $gameOverMenuFont.Dispose()
    $gameOverHeadingFont.Dispose()
    $gameOverGraphics.Dispose()
    $gameOverBitmap.Dispose()
}
