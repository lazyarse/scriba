@echo off
REM Build Scriba Windows installer with NSIS
REM Run in an x64 Native Tools Command Prompt for VS 2022

set QT_PATH=C:\Qt\6.10.3\msvc2022_64
set PATH=%QT_PATH%\bin;%PATH%

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_PATH%" || exit /b 1
cmake --build build -j4 --config Release || exit /b 1
cpack --config build/CPackConfig.cmake -G NSIS || exit /b 1

echo Done. Look for scriba-*-win64.exe
