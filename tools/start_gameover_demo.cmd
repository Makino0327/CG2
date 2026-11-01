@echo off
chcp 65001 >nul
rem プレイ会用の自動プレイを開始する。終了してタイトルへ戻る操作はF6。
if not exist "%~dp0..\generated\outputs\Debug\CG2.exe" (
    echo Debug版を先にビルドしてください。
    pause
    exit /b 1
)
pushd "%~dp0..\project"
start "" "%~dp0..\generated\outputs\Debug\CG2.exe" --gameover-demo
popd
