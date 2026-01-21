@echo off
setlocal enabledelayedexpansion

:: ==============================================================================
:: EA VT-2A - EMU AUDIO
:: ビルドスクリプト (Windows)
:: ==============================================================================

echo ========================================
echo   EA VT-2A - Build Script
echo   EMU AUDIO
echo ========================================
echo.

:: プロジェクトルートディレクトリの設定
set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build

:: ビルドタイプの決定 (デフォルトは Release)
set BUILD_TYPE=Release
if not "%~1"=="" set BUILD_TYPE=%~1

echo Build Type: %BUILD_TYPE%
echo.

:: ビルドディレクトリの作成
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

:: CMake設定
echo [1/3] Configuring with CMake...
cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if %ERRORLEVEL% neq 0 (
    echo.
    echo Error: CMake configuration failed.
    exit /b %ERRORLEVEL%
)

echo.

:: ビルド実行
echo [2/3] Building...
cmake --build . --config %BUILD_TYPE% -j %NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 (
    echo.
    echo Error: Build failed.
    exit /b %ERRORLEVEL%
)

echo.

:: 結果表示
echo [3/3] Build Complete!
echo.
echo Built plugins:
dir /s /b *.vst3 *.dll

echo.
echo ========================================
echo   Build finished successfully!
echo ========================================

exit /b 0
