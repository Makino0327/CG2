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

function Fill-BriefingRect($graphics, [Drawing.Color]$color, [int]$x, [int]$y, [int]$width, [int]$height) {
    $brush = [Drawing.SolidBrush]::new($color)
    try { $graphics.FillRectangle($brush, $x, $y, $width, $height) }
    finally { $brush.Dispose() }
}

try {
    # 1200×128の各行を半分の大きさで表示し、日本語も読みやすくする。
    Save-HudImage 'pause_labels.png' 1200 512 {
        param($graphics)
        $labels = @('PAUSED', 'ゲームに戻る', '操作説明', 'タイトルに戻る')
        $sizes = @(104, 44, 44, 44)
        for ($row = 0; $row -lt $labels.Count; $row++) {
            Draw-HudText $graphics $labels[$row] $sizes[$row] 0 ($row * 128) 128 ([Drawing.Color]::White) $true
        }
    }
    # 開始前とポーズ中で共用する操作説明。見出しとキー一覧だけを描く。
    Save-HudImage 'controls_briefing.png' 960 540 {
        param($graphics)
        $text = [Drawing.Color]::FromArgb(245, 246, 248)
        $line = [Drawing.Color]::FromArgb(63, 69, 77)
        Fill-BriefingRect $graphics $line 0 0 960 540
        Fill-BriefingRect $graphics ([Drawing.Color]::FromArgb(20, 22, 25)) 1 1 958 538
        Draw-HudText $graphics '操作説明' 42 40 33 53 $text $true
        Fill-BriefingRect $graphics $line 40 106 880 1

        $movement = @(
            @('W / A / S / D', '移動'),
            @('マウス移動', '狙う方向を変える'),
            @('E', '発電機を起動'),
            @('M', 'マップの拡大・縮小'),
            @('Tab', 'ポーズ')
        )
        $combat = @(
            @('右クリック長押し', '銃を構える'),
            @('左クリック', '射撃（構え中）／近接攻撃'),
            @('Q', '武器を切り替える'),
            @('R', 'リロード'),
            @('G', 'グレネードを投げる')
        )
        # 各列でキーと動作を揃え、文字を大きく表示する。
        for ($column = 0; $column -lt 2; $column++) {
            $columnX = 40 + $column * 448
            $rows = if ($column -eq 0) { $movement } else { $combat }
            for ($row = 0; $row -lt $rows.Count; $row++) {
                $rowY = 129 + $row * 62
                Draw-HudText $graphics $rows[$row][0] 19 $columnX $rowY 48 $text $true
                Draw-HudText $graphics $rows[$row][1] 22 ($columnX + 170) $rowY 48 $text
                Fill-BriefingRect $graphics $line $columnX ($rowY + 52) 432 1
            }
        }
    }

    # 上段が作戦前、下段がポーズから開いたときの決定ボタン。
    Save-HudImage 'controls_briefing_buttons.png' 336 88 {
        param($graphics)
        $border = [Drawing.Color]::FromArgb(136, 143, 152)
        $fill = [Drawing.Color]::FromArgb(48, 53, 60)
        $white = [Drawing.Color]::White
        $keys = @('Enter / Space', 'Esc / Tab')
        $actions = @('続ける', '戻る')
        for ($row = 0; $row -lt $actions.Count; $row++) {
            $rowY = 44 * $row
            Fill-BriefingRect $graphics $border 0 $rowY 336 44
            Fill-BriefingRect $graphics $fill 1 ($rowY + 1) 334 42
            Draw-HudText $graphics $keys[$row] 17 18 $rowY 44 $white
            Draw-HudText $graphics $actions[$row] 21 180 $rowY 44 $white $true 136
        }
    }
} finally {
    $pauseFamily.Dispose()
}
