@echo off
setlocal

set "MAIN_PROJECT=%~dp0uc_online2.vcxproj"
set "CORE_PROJECT=%~dp0uc_online2_core.vcxproj"

REM Find MSBuild
set "MSBUILD="
for /f "delims=" %%i in ('dir /b /s "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" 2^>nul') do (
    if not defined MSBUILD set "MSBUILD=%%i"
)

if not defined MSBUILD (
    echo [ERROR] MSBuild not found.
    echo.
    echo You need Visual Studio Build Tools 2022 or later.
    echo Install it here: https://visualstudio.microsoft.com/visual-cpp-build-tools/
    echo.
    echo Select "Desktop development with C++" during installation.
    echo.
    pause
    exit /b 1
)

echo Found MSBuild: %MSBUILD%
echo.

echo ========================================
echo  Building Core DLL (uc_online2_core.dll)
echo ========================================

"%MSBUILD%" "%CORE_PROJECT%" -p:Configuration=Release -p:Platform=Win32 -m
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Core x86 build failed.
    pause
    exit /b 1
)
echo Core x86 build succeeded: build\x86\uc_online2_core.dll
echo.

echo ========================================
echo  Building Core DLL (uc_online2_core.dll)
echo ========================================
"%MSBUILD%" "%CORE_PROJECT%" -p:Configuration=Release -p:Platform=x64 -m
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Core x64 build failed.
    pause
    exit /b 1
)
echo Core x64 build succeeded: build\x64\uc_online2_core.dll
echo.

echo ========================================
echo  Building Main DLL (steam_api.dll)
echo ========================================
"%MSBUILD%" "%MAIN_PROJECT%" -p:Configuration=Release -p:Platform=Win32 -m
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Main x86 build failed.
    pause
    exit /b 1
)
echo Main x86 build succeeded: build\x86\steam_api.dll
echo.

echo ========================================
echo  Building Main DLL (steam_api64.dll)
echo ========================================
"%MSBUILD%" "%MAIN_PROJECT%" -p:Configuration=Release -p:Platform=x64 -m
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Main x64 build failed.
    pause
    exit /b 1
)
echo Main x64 build succeeded: build\x64\steam_api64.dll
echo.

echo ========================================
echo  All builds completed successfully
echo ========================================
echo  Core x86: build\x86\uc_online2_core.dll
echo  Core x64: build\x64\uc_online2_core.dll
echo  Main x86: build\x86\steam_api.dll
echo  Main x64: build\x64\steam_api64.dll
echo ========================================

pause