# チュートリアルの案内文とタイトルのボタン文字を、ゲームで使う透過画像として生成する
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$projectDirectory = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../project'))
$hudDirectory = Join-Path $projectDirectory 'Resources/hud'
$titleDirectory = Join-Path $projectDirectory 'Resources/title'
$family = [System.Drawing.FontFamily]::new('Meiryo')

# 文字の輪郭を基準に置く。入りきらない長さの時は、幅に合わせて縮める
function Draw-FitText($graphics, [string]$text, [float]$size, [float]$x, [float]$y, [float]$height,
    [float]$maxWidth, [System.Drawing.Color]$color, [bool]$bold = $false, [bool]$center = $false) {
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $transform = [System.Drawing.Drawing2D.Matrix]::new()
    $brush = [System.Drawing.SolidBrush]::new($color)
    try {
        $style = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }
        $path.AddString($text, $family, [int]$style, $size, [System.Drawing.PointF]::new(0, 0),
            [System.Drawing.StringFormat]::GenericTypographic)
        $bounds = $path.GetBounds()
        $scale = [Math]::Min(1.0, $maxWidth / $bounds.Width)
        $width = $bounds.Width * $scale
        # 文字の高さは行の中央へ。縮めた場合も同じ位置を基準にする
        $offsetX = if ($center) { ($maxWidth - $width) / 2 } else { 0 }
        $transform.Translate($x + $offsetX, $y + ($height - $bounds.Height * $scale) / 2)
        $transform.Scale($scale, $scale)
        $transform.Translate(-$bounds.X, -$bounds.Y)
        $path.Transform($transform)
        $graphics.FillPath($brush, $path)
        return $width
    } finally {
        $brush.Dispose()
        $transform.Dispose()
        $path.Dispose()
    }
}

function Save-Image([string]$path, [int]$width, [int]$height, [scriptblock]$draw) {
    $bitmap = [System.Drawing.Bitmap]::new($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        & $draw $graphics
        $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Output $path
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

# 各課題は「見出し・説明1・説明2」の順。GamePlayTutorial.cpp の行番号と揃える
$steps = @(
    @('移動してみよう', 'W A S D で移動、マウスで向きを変えます', '奥の光っている場所まで歩いてみよう'),
    @('ナイフで攻撃しよう', '銃を構えていない時は、左クリックでナイフ攻撃', '敵に近づいて倒そう（この敵は動きません）'),
    @('弾薬を拾って撃ってみよう', '弾薬は近づくと拾えます。右クリック長押しで構え、左クリックで撃つ', 'Q で銃を切り替え、弾が空なら R でリロード'),
    @('的を撃ってみよう', '好きな銃で、奥にいる3体の敵を倒そう', '構えている間は、歩く速さが遅くなります'),
    @('グレネードを投げよう', 'G でマウスの方向へグレネードを投げます', '固まっている敵をまとめて倒そう'),
    @('発電機を起動しよう', '発電機の近くで E を押すと起動します（15秒）', '起動中は音で敵が集まります。倒して守り切ろう'),
    @('ポータルに入ろう', '本番では、全ての発電機を起動すると道が開きます', '最後に光るポータルに入ればクリアです')
)
$checks = @('光っている場所まで歩く', '敵を倒す', '弾薬を拾う', '3種類の銃で撃つ', '発電機を起動する', 'ポータルに入る')
$banners = @('クリア！', 'やられた…', 'チュートリアル完了！')
$bannerNotes = @('ドアが開いた！ 次の部屋へ進もう', 'この部屋の入口からやり直そう', 'おつかれさま！ タイトルに戻ります')

$rowWidth = 1024
$rowHeight = 64
$rowCount = $steps.Count * 3 + $checks.Count + $banners.Count + $bannerNotes.Count
$white = [System.Drawing.Color]::White
$gray = [System.Drawing.Color]::FromArgb(170, 178, 188)

try {
    # 課題パネルの文字は2倍の大きさで描き、ゲームでは半分に縮めて表示する
    Save-Image (Join-Path $hudDirectory 'tutorial_labels.png') $rowWidth ($rowCount * $rowHeight) {
        param($graphics)
        $row = 0
        for ($step = 0; $step -lt $steps.Count; $step++) {
            $top = $row * $rowHeight
            $stepWidth = Draw-FitText $graphics ("STEP $($step + 1)/$($steps.Count)") 26 0 $top $rowHeight 200 $gray $true
            [void](Draw-FitText $graphics $steps[$step][0] 42 ($stepWidth + 24) $top $rowHeight (880 - $stepWidth) $white $true)
            $row++
            for ($line = 1; $line -le 2; $line++) {
                [void](Draw-FitText $graphics $steps[$step][$line] 32 0 ($row * $rowHeight) $rowHeight 1000 $white)
                $row++
            }
        }
        foreach ($check in $checks) {
            [void](Draw-FitText $graphics $check 32 0 ($row * $rowHeight) $rowHeight 860 $white $true)
            $row++
        }
        # お知らせは画面中央に等倍で出すので、行の中央へ揃える
        foreach ($banner in $banners) {
            [void](Draw-FitText $graphics $banner 52 0 ($row * $rowHeight) $rowHeight $rowWidth $white $true $true)
            $row++
        }
        foreach ($note in $bannerNotes) {
            [void](Draw-FitText $graphics $note 28 0 ($row * $rowHeight) $rowHeight $rowWidth $white $false $true)
            $row++
        }
    }

    # タイトルのボタンは menu_labels.png と同じ512×64の中央揃えにする
    Save-Image (Join-Path $titleDirectory 'tutorial_label.png') 512 64 {
        param($graphics)
        [void](Draw-FitText $graphics 'チュートリアル' 36 0 0 64 512 $white $true $true)
    }
} finally {
    $family.Dispose()
}
