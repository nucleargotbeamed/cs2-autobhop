@echo off
cd /d "%~dp0"

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set vs=%%i
set cmake="%vs%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if not exist %cmake% (
    echo visual studio with c++ tools not found
    pause
    exit /b 1
)

%cmake% -B build -S . -A x64 >nul
%cmake% --build build --config Release

if errorlevel 1 (echo build failed) else (echo built dlls\bhop.dll)
pause
