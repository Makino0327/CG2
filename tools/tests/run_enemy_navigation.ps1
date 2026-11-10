$ErrorActionPreference = 'Stop'
$taskRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location $taskRoot
try {
    # 日本語コメントを保ち、描画に依存しない実際のAI関数だけを取り出す
    $source = [System.IO.File]::ReadAllText((Join-Path $taskRoot 'project/game/enemy/Enemy.cpp'))
    $namespaceStart = $source.IndexOf('namespace {')
    $namespaceEnd = $source.IndexOf('void Enemy::Initialize(')
    $methods = $source.Substring($namespaceStart, $namespaceEnd - $namespaceStart)
    $names = @('SetTargetPosition', 'SetNavMesh', 'FindNavMeshTriangle',
        'GetNavMeshTriangleCenter', 'FindNavMeshPath', 'CalculateNavMeshChaseDirection',
        'IsSegmentBlockedByWall', 'IsPositionBlockedByWall', 'CalculateChaseDirection',
        'CheckTargetInSight')
    foreach ($name in $names) {
        $pattern = '(?ms)^(?:void|bool|int|Vector3) Enemy::' + $name + '\(.*?^\}'
        $match = [regex]::Match($source, $pattern)
        if (!$match.Success) { throw "AI function not found: $name" }
        $methods += $match.Value + "`r`n"
    }
    $outputDirectory = Join-Path $taskRoot 'generated/tests'
    [System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $outputDirectory 'enemy_navigation_methods.h'),
        $methods, [System.Text.UTF8Encoding]::new($false))
    # 標準の開発環境を読み込み、毎回最新の実装からテストをビルドする
    $commands = @'
@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /utf-8 /W3 /O2 /Gy /Zc:inline /Iproject/app /Iproject/externals/imgui /Fogenerated/tests/ /Fegenerated/tests/test_enemy_navigation.exe tools/tests/test_enemy_navigation.cpp project/engine/math/Math.cpp /link /OPT:REF /INCREMENTAL:NO
if errorlevel 1 exit /b %errorlevel%
generated\tests\test_enemy_navigation.exe
exit /b %errorlevel%
'@
    [System.IO.File]::WriteAllText((Join-Path $outputDirectory 'run_enemy_navigation.cmd'),
        $commands, [System.Text.Encoding]::ASCII)
    & cmd /c generated\tests\run_enemy_navigation.cmd
    if ($LASTEXITCODE -ne 0) { throw "Enemy navigation tests failed: $LASTEXITCODE" }
} finally {
    Pop-Location
}
