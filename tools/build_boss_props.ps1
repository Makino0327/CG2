# 木箱の板・補強材と、突進の予告帯をゲームのOBJモデルとして生成する。
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$resources = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../project/Resources'))
$crateDirectory = Join-Path $resources 'wooden_crate'
$bossDirectory = Join-Path $resources 'boss'
New-Item -ItemType Directory -Force -Path $crateDirectory | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)

# 各面に同じ木目を貼り、上から見ても木箱だと分かる補強板を描く。
$bitmap = [Drawing.Bitmap]::new(256, 256)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$random = [Random]::new(197)
try {
    $graphics.Clear([Drawing.Color]::FromArgb(126, 78, 36))
    for ($plank = 0; $plank -lt 5; $plank++) {
        $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(135 + $plank * 5, 84 + $plank * 3, 40))
        $graphics.FillRectangle($brush, $plank * 52, 0, 49, 256)
        $brush.Dispose()
    }
    $grain = [Drawing.Pen]::new([Drawing.Color]::FromArgb(80, 67, 36, 15), 1)
    for ($line = 0; $line -lt 180; $line++) {
        $x = $random.Next(256)
        $y = $random.Next(256)
        $graphics.DrawLine($grain, $x, $y, $x + $random.Next(-2, 3), [Math]::Min(255, $y + $random.Next(15, 100)))
    }
    $grain.Dispose()
    $braceShadow = [Drawing.Pen]::new([Drawing.Color]::FromArgb(69, 40, 18), 35)
    $brace = [Drawing.Pen]::new([Drawing.Color]::FromArgb(185, 123, 61), 25)
    $graphics.DrawLine($braceShadow, 16, 240, 240, 16)
    $graphics.DrawLine($brace, 16, 240, 240, 16)
    $braceShadow.Dispose()
    $brace.Dispose()
    $frame = [Drawing.Pen]::new([Drawing.Color]::FromArgb(188, 128, 66), 23)
    $graphics.DrawRectangle($frame, 13, 13, 230, 230)
    $frame.Dispose()
    $nail = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(48, 43, 37))
    foreach ($x in @(15, 241)) {
        foreach ($y in @(15, 241)) { $graphics.FillEllipse($nail, $x - 3, $y - 3, 6, 6) }
    }
    $nail.Dispose()
    $bitmap.Save((Join-Path $crateDirectory 'wooden_crate.png'), [Drawing.Imaging.ImageFormat]::Png)
} finally { $graphics.Dispose(); $bitmap.Dispose() }

# 対称な立方体に、各面共通の0～1のUVを付ける。
$crate = [Text.StringBuilder]::new()
[void]$crate.AppendLine("mtllib wooden_crate.mtl`no WoodenCrate")
foreach ($vertex in @('1 1 -1','1 -1 -1','1 1 1','1 -1 1','-1 1 -1','-1 -1 -1','-1 1 1','-1 -1 1')) {
    [void]$crate.AppendLine("v $vertex")
}
[void]$crate.AppendLine("vt 0 0`nvt 1 0`nvt 1 1`nvt 0 1")
foreach ($normal in @('0 1 0','0 -1 0','1 0 0','-1 0 0','0 0 1','0 0 -1')) {
    [void]$crate.AppendLine("vn $normal")
}
$faces = @(@(5,7,3,1),@(2,4,8,6),@(1,3,4,2),@(7,5,6,8),@(3,7,8,4),@(5,1,2,6))
for ($face = 0; $face -lt $faces.Count; $face++) {
    $v = $faces[$face]
    $n = $face + 1
    [void]$crate.AppendLine("f $($v[0])/1/$n $($v[1])/2/$n $($v[2])/3/$n")
    [void]$crate.AppendLine("f $($v[0])/1/$n $($v[2])/3/$n $($v[3])/4/$n")
}
[IO.File]::WriteAllText((Join-Path $crateDirectory 'wooden_crate.obj'), $crate.ToString(), $utf8)
[IO.File]::WriteAllText((Join-Path $crateDirectory 'wooden_crate.mtl'), "newmtl Wood`nmap_Kd wooden_crate.png`n", $utf8)

# ローダーでZが反転するため、元のOBJでは進行方向を負のZへ向ける。
$script:warning = [Text.StringBuilder]::new()
$script:warningVertex = 0
[void]$warning.AppendLine("mtllib charge_warning.mtl`no ChargeWarning`nvt 0.5 0.5`nvn 0 1 0")
function Add-WarningTriangle($a, $b, $c) {
    foreach ($vertex in @($a, $b, $c)) { [void]$script:warning.AppendLine("v $vertex") }
    $i = $script:warningVertex + 1
    [void]$script:warning.AppendLine("f $i/1/1 $($i+1)/1/1 $($i+2)/1/1")
    $script:warningVertex += 3
}
function Add-WarningQuad($x0, $z0, $x1, $z1, $height) {
    Add-WarningTriangle "$x0 $height $z0" "$x1 $height $z0" "$x1 $height $z1"
    Add-WarningTriangle "$x0 $height $z0" "$x1 $height $z1" "$x0 $height $z1"
}
Add-WarningQuad -1 0 1 -1 0
Add-WarningQuad -1 0 -0.94 -1 0.006
Add-WarningQuad 0.94 0 1 -1 0.006
for ($arrow = 0; $arrow -lt 5; $arrow++) {
    $tail = -0.08 - $arrow * 0.18
    $tip = $tail - 0.1
    Add-WarningTriangle "-0.35 0.012 $tail" "0.35 0.012 $tail" "0 0.012 $tip"
}
[IO.File]::WriteAllText((Join-Path $bossDirectory 'charge_warning.obj'), $warning.ToString(), $utf8)
[IO.File]::WriteAllText((Join-Path $bossDirectory 'charge_warning.mtl'), "newmtl Warning`nmap_Kd ../white2x2.png`n", $utf8)
