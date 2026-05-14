@echo off
chcp 65001 >nul
echo ========================================
echo     复制 FFmpeg DLL 到编译目录
echo ========================================
echo.

set "SOURCE_DIR=ffmpeg\bin"
set "TARGET_DIR="

echo 正在查找编译输出目录...
echo.

REM 查找常见的构建目录
if exist "build-ScreenRecorder-Desktop_Qt_5_12_11_MinGW_32_bit-Debug\debug\" (
    set "TARGET_DIR=build-ScreenRecorder-Desktop_Qt_5_12_11_MinGW_32_bit-Debug\debug\"
    goto found
)

if exist "build-ScreenRecorder-Desktop_Qt_5_12_11_MinGW_32_bit-Release\release\" (
    set "TARGET_DIR=build-ScreenRecorder-Desktop_Qt_5_12_11_MinGW_32_bit-Release\release\"
    goto found
)

if exist "debug\" (
    set "TARGET_DIR=debug\"
    goto found
)

if exist "release\" (
    set "TARGET_DIR=release\"
    goto found
)

echo [警告] 未找到自动识别的目录
echo.
echo 请手动复制 ffmpeg\bin\ 下的所有 DLL
echo 到你的 exe 文件同级目录
echo.
goto end

:found
echo [OK] 找到目标目录: %TARGET_DIR%
echo.
echo 正在复制 DLL 文件...

xcopy /Y /Q "%SOURCE_DIR%\*.dll" "%TARGET_DIR%\"

echo.
echo ========================================
echo  [OK] DLL 复制完成！
echo ========================================
echo.
echo 现在可以运行程序了！
echo.
pause
exit /b 0

:end
pause
