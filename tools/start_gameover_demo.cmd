@echo off
chcp 65001 >nul
rem ボス撃破とクリア画面を自動で繰り返す。F6でタイトルへ戻る。
if not exist "%~dp0..\generated\outputs\Debug\CG2.exe" (
    echo Debug版を先にビルドしてください。
    pause
    exit /b 1
)
pushd "%~dp0..\project"
start "" "%~dp0..\generated\outputs\Debug\CG2.exe" --clear-demo
popd
