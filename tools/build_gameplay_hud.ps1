# 目的表示と操作説明を、ゲーム内のSpriteで使える透過画像へ描画する
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$hudDirectory = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../project/Resources/hud'))
New-Item -ItemType Directory -Force -Path $hudDirectory | Out-Null
$hudFamily = [System.Drawing.FontFamily]::new('Meiryo')

function Draw-HudText($graphics, [string]$text, [float]$size, [float]$x, [float]$y,
    [float]$height, [System.Drawing.Color]$color, [bool]$bold = $false, [float]$centerWidth = 0) {
    # 日本語の輪郭を基準に配置して、フォントの大きな行間による文字切れを防ぐ
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $transform = [System.Drawing.Drawing2D.Matrix]::new()
    $brush = [System.Drawing.SolidBrush]::new($color)
    try {
        $style = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }
        $path.AddString($text, $hudFamily, [int]$style, $size,
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
        $path = Join-Path $hudDirectory $name
        $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Output $path
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

try {
    # 1行640×128を半分の大きさで表示する。上段が発電機、下段がポータル
    Save-HudImage 'objective_labels.png' 640 256 {
        param($graphics)
        $labels = @('ジェネレーターを起動しよう', 'ポータルに入ろう')
        for ($row = 0; $row -lt $labels.Count; $row++) {
            Draw-HudText $graphics '目標' 28 0 ($row * 128 + 8) 32 ([System.Drawing.Color]::FromArgb(185, 193, 202)) $true
            Draw-HudText $graphics $labels[$row] 40 0 ($row * 128 + 64) 48 ([System.Drawing.Color]::White) $true
        }
    }

    # 数字は1文字ごとに切り出すため、すべて同じ幅にしておく
    Save-HudImage 'count_glyphs.png' 352 64 {
        param($graphics)
        $glyphs = '0123456789/'
        for ($index = 0; $index -lt $glyphs.Length; $index++) {
            Draw-HudText $graphics ([string]$glyphs[$index]) 40 ($index * 32) 0 64 ([System.Drawing.Color]::White) $true 32
        }
    }

    # 操作説明も2倍解像度で作り、半透明の背景で明るい床の上でも読めるようにする
    Save-HudImage 'controls_guide.png' 648 464 {
        param($graphics)
        $graphics.ScaleTransform(2, 2)
        $panelBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(185, 5, 6, 8))
        try {
            $graphics.FillRectangle($panelBrush, 0, 0, 324, 232)
        } finally {
            $panelBrush.Dispose()
        }
        Draw-HudText $graphics '操作方法' 14 14 10 18 ([System.Drawing.Color]::FromArgb(180, 189, 200)) $true
        # PlayerとGamePlaySceneで使用している実際の操作キーに合わせる
        $rows = @(
            @('W A S D', '移動'),
            @('マウス', '狙う方向'),
            @('右クリック長押し', '銃を構える'),
            @('左クリック', '近接攻撃／構え中は射撃'),
            @('Q / R', '銃の切替 / リロード'),
            @('G', 'グレネード'),
            @('E', '起動（近づいて押す）'),
            @('M', 'マップ拡大 / 縮小')
        )
        for ($row = 0; $row -lt $rows.Count; $row++) {
            Draw-HudText $graphics $rows[$row][0] 14 14 (32 + $row * 24) 24 ([System.Drawing.Color]::White) $true
            Draw-HudText $graphics $rows[$row][1] 15 140 (32 + $row * 24) 24 ([System.Drawing.Color]::FromArgb(220, 225, 232))
        }
    }
} finally {
    $hudFamily.Dispose()
}
